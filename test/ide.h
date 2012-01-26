
struct gdrom_session {
    int track;
    unsigned int lba;
};

struct gdrom_track {
    int mode;
    unsigned int lba;
};

struct gdrom_toc {
    struct gdrom_track track[99];
    unsigned int first_lba;
    unsigned int last_lba;
    unsigned int leadout_lba;
};

int ide_init();

int ide_test_ready();

int ide_sense_error( char *buf );

int ide_get_sense_code();

/**
 * Retrieve session information. If session == 0, returns the
 * end-of-disc information instead.
 */
int ide_get_session( int session, struct gdrom_session *session_data );

/**
 * Read 1 or more sectors in PIO mode
 */
int ide_read_sector_pio( unsigned int sector, unsigned int count, int mode,
			 char *buf, int length );
int ide_read_sector_dma( unsigned int sector, unsigned int count, int mode,
			 char *buf, int length );
