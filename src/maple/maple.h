/**
 * Maple bus routines
 */
#ifndef dream_maple_H
#define dream_maple_H 1

#define MAPLE_CMD_INFO        1  /* Request device information */
#define MAPLE_CMD_EXT_INFO    2  /* Request extended information */
#define MAPLE_CMD_RESET       3  /* Reset device */
#define MAPLE_CMD_SHUTDOWN    4  /* Shutdown device */
#define MAPLE_CMD_GET_COND    9  /* Get condition */
#define MAPLE_CMD_MEM_INFO    10 /* Get memory information */
#define MAPLE_CMD_READ_BLOCK  11 /* Block read */
#define MAPLE_CMD_WRITE_BLOCK 12 /* Block write */
#define MAPLE_CMD_SET_COND    14 /* Set condition */
#define MAPLE_RESP_INFO       5  /* Device information response */
#define MAPLE_RESP_EXT_INFO   6  /* Extended device information response */
#define MAPLE_RESP_ACK        7  /* Acknowledge command */
#define MAPLE_RESP_DATA       8  /* Bytes read */
#define MAPLE_ERR_NO_RESPONSE -1 /* Device did not respond */
#define MAPLE_ERR_FUNC_UNSUP  -2 /* Function code unsupported */
#define MAPLE_ERR_CMD_UNKNOWN -3 /* Command code unknown */
#define MAPLE_ERR_RETRY       -4 /* Retry command */
#define MAPLE_ERR_FILE        -5 /* File error? */

#define MAPLE_FUNC_CONTROLLER 0x01000000
#define MAPLE_FUNC_MEMORY     0x02000000
#define MAPLE_FUNC_LCD        0x04000000
#define MAPLE_FUNC_CLOCK      0x08000000
#define MAPLE_FUNC_MICROPHONE 0x10000000
#define MAPLE_FUNC_AR_GUN     0x20000000
#define MAPLE_FUNC_KEYBOARD   0x40000000
#define MAPLE_FUNC_LIGHT_GUN  0x80000000
#define MAPLE_FUNC_PURU_PURU  0x00010000
#define MAPLE_FUNC_MOUSE      0x00020000

#define MAPLE_DEVICE_TAG 0x4D41504C
#define MAPLE_DEVICE(x) ((maple_device_t)x)

typedef struct maple_device {
    uint32_t _tag;
    unsigned char ident[112];
    unsigned char version[80];
    int (*reset)(struct maple_device *dev);
    int (*shutdown)(struct maple_device *dev);
    int (*get_condition)(struct maple_device *dev,
                         int function, char *outbuf, int *buflen);
    int (*set_condition)(struct maple_device *dev,
                         int function, char *inbuf, int buflen);
    int (*read_block)(struct maple_device *dev,
                      int function, uint32_t block, char *outbuf, int *buflen);
    int (*write_block)(struct maple_device *dev,
                       int function, uint32_t block, char *inbuf, int buflen);
    void (*attach)(struct maple_device *dev);
    void (*detach)(struct maple_device *dev);
} *maple_device_t;

maple_device_t controller_new(void);

void maple_handle_buffer( uint32_t buffer );
void maple_attach_device( maple_device_t dev, unsigned int port, unsigned int periph );
void maple_detach_device( unsigned int port, unsigned int periph );

#endif /* !dream_maple_H */
