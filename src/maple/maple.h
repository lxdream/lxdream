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
#define MAPLE_CMD_BLOCK_READ  11 /* Block read */
#define MAPLE_CMD_BLOCK_WRITE 12 /* Block write */
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

#define MAPLE_FUNC_CONTROLLER 0x001
#define MAPLE_FUNC_MEMORY     0x002
#define MAPLE_FUNC_LCD        0x004
#define MAPLE_FUNC_CLOCK      0x008
#define MAPLE_FUNC_MICROPHONE 0x010
#define MAPLE_FUNC_AR_GUN     0x020
#define MAPLE_FUNC_KEYBOARD   0x040
#define MAPLE_FUNC_LIGHT_GUN  0x080
#define MAPLE_FUNC_PURU_PURU  0x100
#define MAPLE_FUNC_MOUSE      0x200

void maple_handle_buffer( uint32_t buffer );


struct maple_device_t {
   

};


#endif /* !dream_maple_H */
