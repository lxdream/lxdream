#include "dream.h"
#include "aica.h"
#define MMIO_IMPL
#include "aica.h"

MMIO_REGION_DEFFNS( AICA0 )
MMIO_REGION_DEFFNS( AICA1 )
MMIO_REGION_DEFFNS( AICA2 )

void aica_init( void )
{
    register_io_regions( mmio_list_spu );
    MMIO_NOTRACE(AICA0);
    MMIO_NOTRACE(AICA1);
}

void aica_reset( void )
{

}

