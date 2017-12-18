#include "stdafx.h"
#include "mem_alloc.h"

#ifdef VEEAMSNAP_MEMORY_LEAK_CONTROL

spinlock_t vmem_count_lock;
volatile size_t vmem_max_allocated;
volatile size_t vmem_current_allocated;

atomic_t g_mem_cnt;
atomic_t g_vmem_cnt;

void dbg_mem_init(void )
{
	spin_lock_init( &vmem_count_lock );

	vmem_max_allocated = 0;
	vmem_current_allocated = 0;

	atomic_set( &g_mem_cnt, 0 );
	atomic_set( &g_vmem_cnt, 0 );
}

void dbg_mem_print_state( void )
{
	pr_warn( "\n" );
	pr_warn( "%s:\n", __FUNCTION__ );

	pr_warn( "mem_cnt=%d\n", atomic_read( &g_mem_cnt ) );
	pr_warn( "vmem_cnt=%d\n", atomic_read( &g_vmem_cnt ) );
}

size_t dbg_vmem_get_max_usage( void )
{
	return vmem_max_allocated;
}


volatile long dbg_mem_track = false;
void dbg_mem_track_on( void )
{
	dbg_mem_track = true;
	log_traceln( ".");
}
void dbg_mem_track_off( void )
{
	dbg_mem_track = false;
	log_traceln( "." );
}


void dbg_kfree( const void *ptr )
{
	if (dbg_mem_track){
		log_traceln_p( "kmem ", ptr );
	}
	if (ptr){
		atomic_dec( &g_mem_cnt );
		kfree( ptr );
	}
}


void * dbg_kzalloc( size_t size, gfp_t flags )
{
	void* ptr = kzalloc( size, flags );
	if (ptr)
		atomic_inc( &g_mem_cnt );

	if (dbg_mem_track){
		log_traceln_p( "kmem ", ptr );
	}
	return ptr;
}

void * dbg_kmalloc( size_t size, gfp_t flags )
{
	void* ptr = kmalloc( size, flags );
	if (ptr)
		atomic_inc( &g_mem_cnt );

	if (dbg_mem_track){
		log_traceln_p( "kmem ", ptr );
	}
	return ptr;
}


void * dbg_vmalloc( size_t size )
{
	void* ptr = vmalloc( size );
	if (ptr == NULL)
		return ptr;

	atomic_inc( &g_vmem_cnt );

	if (dbg_mem_track){
		log_traceln_p( "vmem ", ptr );
	}

	spin_lock( &vmem_count_lock );
	vmem_current_allocated += size;
	if (vmem_current_allocated > vmem_max_allocated)
		vmem_max_allocated = vmem_current_allocated;
	spin_unlock( &vmem_count_lock );

	return ptr;
}

void * dbg_vzalloc( size_t size )
{
	void * ptr = dbg_vmalloc( size );
	if (ptr == NULL)
		return ptr;

	memset( ptr, 0, size );

	return ptr;
}

void dbg_vfree( const void *ptr, size_t size )
{
	if (dbg_mem_track){
		log_traceln_p( "vmem ", ptr );
	}
	if (ptr){
		atomic_dec( &g_vmem_cnt );

		vfree( ptr );

		spin_lock( &vmem_count_lock );
		vmem_current_allocated -= size;
		spin_unlock( &vmem_count_lock );
	}
}

#endif

void * dbg_kmalloc_huge( size_t max_size, size_t min_size, gfp_t flags, size_t* p_allocated_size )
{
	void * ptr = NULL;

	do{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,13,0)
		ptr = dbg_kmalloc( max_size, flags | __GFP_NOWARN | __GFP_REPEAT );
#else
		ptr = dbg_kmalloc( max_size, flags | __GFP_NOWARN | __GFP_RETRY_MAYFAIL );
#endif
		if (ptr != NULL){
			*p_allocated_size = max_size;
#ifdef VEEAMSNAP_MEMORY_LEAK_CONTROL
			if (dbg_mem_track){
				log_traceln_p( "kmem ", ptr );
			}
#endif
			return ptr;
		}
		log_errorln_sz( "Cannot to allocate buffer size=", max_size );
		max_size = max_size >> 1;
	} while (max_size >= min_size);
	log_errorln( "Failed to allocate buffer." );
	return NULL;
}

