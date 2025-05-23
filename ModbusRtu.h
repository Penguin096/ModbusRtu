#ifndef MODBUSRTU_h
#define MODBUSRTU_h
/**
 * @file 	ModbusRtu.h
 * @version     1.0
 * @date        2023.09.01
 * @author 	Pavel Shirobokov
 *
 *  Further information:
 *  http://modbus.org/
 *  http://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf
 *
 * @defgroup setup Modbus Object Instantiation/Initialization
 * @defgroup loop Modbus Object Management
 * @defgroup buffer Modbus Buffer Management
 * @defgroup discrete Modbus Function Codes for Discrete Coils/Inputs
 * @defgroup register Modbus Function Codes for Holding/Input Registers
 *
 */

#include <inttypes.h>

#ifdef Arduino_h
#include "Arduino.h"
#else
#define millis() HAL_GetTick()
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define LOW 0x0
#define HIGH 0x1
#define word(h, l) ((uint16_t)((h << 8) | l))
#endif

#ifdef STM32F1xx
#include "../../../../src/main.h"
#elif STM32G474xx
#include "../../../../src/Core/Inc/main.h"
#elif STM32F1
#include "../../../../src/Inc/main.h"
#endif

/**
 * @struct modbus_t
 * @brief
 * Master query structure:
 * This includes all the necessary fields to make the Master generate a Modbus query.
 * A Master may keep several of these structures and send them cyclically or
 * use them according to program needs.
 */
typedef struct
{
    uint8_t u8id;        /*!< Slave address between 1 and 247. 0 means broadcast */
    uint8_t u8fct;       /*!< Function code: 1, 2, 3, 4, 5, 6, 15 or 16 */
    uint16_t u16RegAdd;  /*!< Address of the first register to access at slave/s */
    uint16_t u16CoilsNo; /*!< Number of coils or registers to access */
    uint16_t *au16reg;   /*!< Pointer to memory image in master */
} modbus_t;

enum
{
    RESPONSE_SIZE = 6,
    EXCEPTION_SIZE = 3,
    CHECKSUM_SIZE = 2
};

/**
 * @enum MESSAGE
 * @brief
 * Indexes to telegram frame positions
 */
enum MESSAGE
{
    ID = 0,  //!< ID field
    FUNC,    //!< Function code position
    ADD_HI,  //!< Address high byte
    ADD_LO,  //!< Address low byte
    NB_HI,   //!< Number of coils or registers high byte
    NB_LO,   //!< Number of coils or registers low byte
    BYTE_CNT //!< byte counter
};

/**
 * @enum MB_FC
 * @brief
 * Modbus function codes summary.
 * These are the implement function codes either for Master or for Slave.
 *
 * @see also fctsupported
 * @see also modbus_t
 */
enum MB_FC
{
    MB_FC_NONE = 0,                     /*!< null operator */
    MB_FC_READ_COILS = 1,               /*!< FCT=1 -> read coils or digital outputs */
    MB_FC_READ_DISCRETE_INPUT = 2,      /*!< FCT=2 -> read digital inputs */
    MB_FC_READ_REGISTERS = 3,           /*!< FCT=3 -> read registers or analog outputs */
    MB_FC_READ_INPUT_REGISTER = 4,      /*!< FCT=4 -> read analog inputs */
    MB_FC_WRITE_COIL = 5,               /*!< FCT=5 -> write single coil or output */
    MB_FC_WRITE_REGISTER = 6,           /*!< FCT=6 -> write single register */
    MB_FC_DIAGNOSTIC = 8,               /*!< FCT=8 -> diagnostic */
    MB_FC_WRITE_MULTIPLE_COILS = 15,    /*!< FCT=15 -> write multiple coils or outputs */
    MB_FC_WRITE_MULTIPLE_REGISTERS = 16 /*!< FCT=16 -> write multiple registers */
};

enum COM_STATES
{
    COM_IDLE = 0,
    COM_WAITING = 1

};

enum ERR_LIST
{
    ERR_NOT_MASTER = -1,
    ERR_POLLING = -2,
    ERR_BUFF_OVERFLOW = -3,
    ERR_BAD_CRC = -4,
    ERR_EXCEPTION = -5
};

enum
{
    NO_REPLY = 255,
    EXC_FUNC_CODE = 1,
    EXC_ADDR_RANGE = 2,
    EXC_REGS_QUANT = 3,
    EXC_EXECUTE = 4
};

const unsigned char fctsupported[] =
    {
        MB_FC_READ_COILS,
        MB_FC_READ_DISCRETE_INPUT,
        MB_FC_READ_REGISTERS,
        MB_FC_READ_INPUT_REGISTER,
        MB_FC_WRITE_COIL,
        MB_FC_WRITE_REGISTER,
        MB_FC_DIAGNOSTIC,
        MB_FC_WRITE_MULTIPLE_COILS,
        MB_FC_WRITE_MULTIPLE_REGISTERS};

#define T35 5
#define MAX_BUFFER 64 //!< maximum size for the communication buffer in bytes

/**
 * @class Modbus
 * @brief
 * Arduino class library for communicating with Modbus devices over
 * USB/RS232/485 (via RTU protocol).
 */
class Modbus
{
private:
#ifdef Arduino_h
    Stream *port = NULL; //!< Pointer to Stream class object (Either HardwareSerial or SoftwareSerial)
#endif
#if defined(STM32F1) || defined(STM32F1xx) || defined(STM32G474xx)
    UART_HandleTypeDef *ser_dev = NULL;
#endif
    uint8_t u8id;      //!< 0=master, 1..247=slave number
    uint8_t u8txenpin; //!< flow control pin: 0=USB or RS-232 mode, >1=RS-485 mode
    uint8_t u8state;
    uint8_t u8lastError;
    uint8_t au8Buffer[MAX_BUFFER];
    uint8_t u8BufferSize;
    uint8_t u8lastRec;
    uint16_t *au16regs;
    uint16_t u16InCnt, u16OutCnt, u16errCnt;
    uint16_t u16timeOut;
    uint32_t u32time, u32timeOut, u32overTime;
    uint8_t u8regsize;

    void sendTxBuffer();
#ifndef __AVR__
    void sendTxBuffer_HAL();
#endif
    int8_t getRxBuffer();
    uint16_t calcCRC(uint8_t u8length);
    uint8_t validateAnswer();
    uint8_t validateRequest();
    void get_FC1();
    void get_FC3();
    int8_t process_FC1(bool *regs, uint8_t u8size);
    int8_t process_FC3(uint16_t *regs, uint8_t u8size);
    int8_t process_FC5(bool *regs, uint8_t u8size);
    int8_t process_FC6(uint16_t *regs, uint8_t u8size);
    int8_t process_FC8();
    int8_t process_FC15(bool *regs, uint8_t u8size);
    int8_t process_FC16(uint16_t *regs, uint8_t u8size);
    void buildException(uint8_t u8exception); // build exception message

public:
#ifdef Arduino_h
    Modbus(uint8_t u8id, Stream &port, uint8_t u8txenpin = 0);
#endif
    // Deprecated: Use constructor: "Modbus m(0,Serial,0)" instead.
    Modbus(uint8_t u8id = 0, uint8_t u8serno = 0, uint8_t u8txenpin = 0) __attribute__((deprecated));
#if defined(STM32F1) || defined(STM32F1xx) || defined(STM32G474xx)
    Modbus(uint8_t u8id, UART_HandleTypeDef *theSer, uint8_t u8txenpin = 0);
#endif

    void start();
    void setTimeOut(uint16_t u16timeOut);                                                                                                        //!< write communication watch-dog timer
    uint16_t getTimeOut();                                                                                                                       //!< get communication watch-dog timer value
    bool getTimeOutState();                                                                                                                      //!< get communication watch-dog timer state
    int8_t query(modbus_t telegram, uint16_t *au16data);                                                                                         //!< only for master
    int8_t poll();                                                                                                                               //!< cyclic poll for master
    int8_t poll(bool *DO, bool *DI, uint16_t *AI, uint16_t *AO, uint8_t DO_u8size, uint8_t DI_u8size, uint8_t AI_u8size, uint8_t AO_u8size);     //!< cyclic poll for slave
    int8_t poll_IRQ(bool *DO, bool *DI, uint16_t *AI, uint16_t *AO, uint8_t DO_u8size, uint8_t DI_u8size, uint8_t AI_u8size, uint8_t AO_u8size); //!< cyclic poll for slave
#ifndef __AVR__
    int8_t poll_IRQ_HAL(bool *DO, bool *DI, uint16_t *AI, uint16_t *AO, uint8_t DO_u8size, uint8_t DI_u8size, uint8_t AI_u8size, uint8_t AO_u8size);
#endif
    uint16_t getInCnt();  //!< number of incoming messages
    uint16_t getOutCnt(); //!< number of outcoming messages
    uint16_t getErrCnt(); //!< error counter
    uint8_t getID();      //!< get slave ID between 1 and 247
    uint8_t getState();
    uint8_t getLastError();   //!< get last error message
    void setID(uint8_t u8id); //!< write new ID for the slave
    void setTxendPinOverTime(uint32_t u32overTime);
    void end(); //!< finish any communication and release serial communication port
    inline volatile void restart()
    {
#if defined(STM32F1) || defined(STM32F1xx) || defined(STM32G474xx)
        NVIC_SystemReset();
#endif
    }
};
#endif