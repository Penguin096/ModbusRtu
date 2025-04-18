#include "ModbusRtu.h"
#ifndef ARDUINO
#include "main.h"
#endif
#include "string.h"

/* _____PUBLIC FUNCTIONS_____________________________________________________ */

/**
 * @brief
 * Constructor for a Master/Slave.
 *
 * For hardware serial through USB/RS232C/RS485 set port to Serial, Serial1,
 * Serial2, or Serial3. (Numbered hardware serial ports are only available on
 * some boards.)
 *
 * For software serial through RS232C/RS485 set port to a SoftwareSerial object
 * that you have already constructed.
 *
 * ModbusRtu needs a pin for flow control only for RS485 mode. Pins 0 and 1
 * cannot be used.
 *
 * First call begin() on your serial port, and then start up ModbusRtu by
 * calling start(). You can choose the line speed and other port parameters
 * by passing the appropriate values to the port's begin() function.
 *
 * @param u8id   node address 0=master, 1..247=slave
 * @param port   serial port used
 * @param u8txenpin pin for txen RS-485 (=0 means USB/RS232C mode)
 * @ingroup setup
 */
#ifdef Arduino_h
Modbus::Modbus(uint8_t u8id, Stream &port, uint8_t u8txenpin)
{
    this->port = &port;
    this->u8id = u8id;
    this->u8txenpin = u8txenpin;
    this->u16timeOut = 1000;
    this->u32overTime = 500;
}
#endif
extern UART_HandleTypeDef huart1;
/**
 * @brief
 * DEPRECATED constructor for a Master/Slave.
 *
 * THIS CONSTRUCTOR IS ONLY PROVIDED FOR BACKWARDS COMPATIBILITY.
 * USE Modbus(uint8_t, T_Stream&, uint8_t) INSTEAD.
 *
 * @param u8id   node address 0=master, 1..247=slave
 * @param u8serno  serial port used 0..3 (ignored for software serial)
 * @param u8txenpin pin for txen RS-485 (=0 means USB/RS232C mode)
 * @ingroup setup
 * @overload Modbus::Modbus(uint8_t u8id, T_Stream& port, uint8_t u8txenpin)
 */
Modbus::Modbus(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin)
{
    this->u8id = u8id;
    this->u8txenpin = u8txenpin;
    this->u16timeOut = 1000;
    this->u32overTime = 500;

    switch (u8serno)
    {
#if defined(UBRR1H)
    case 1:
        port = &Serial1;
        break;
#endif

#if defined(UBRR2H)
    case 2:
        port = &Serial2;
        break;
#endif

#if defined(UBRR3H)
    case 3:
        port = &Serial3;
        break;
#endif
    case 0:
    default:
#ifdef Arduino_h
        port = &Serial;
#endif
        break;
    }
}
#if defined(STM32F1xx) || defined(STM32F1) || defined(STM32G474xx)
Modbus::Modbus(uint8_t u8id, UART_HandleTypeDef *theSer, uint8_t u8txenpin)
{
    this->u8id = u8id;
    this->u8txenpin = u8txenpin;
    this->u16timeOut = 1000;
    this->u32overTime = 500;

    ser_dev = theSer;
}
#endif

/**
 * @brief
 * Start-up class object.
 *
 * @ingroup setup
 */
void Modbus::start()
{
    if (u8txenpin > 1) // pin 0 & pin 1 are reserved for RX/TX
    {
// return RS485 transceiver to transmit mode
#ifdef Arduino_h
        pinMode(u8txenpin, OUTPUT);
        digitalWrite(u8txenpin, LOW);
#endif
    }
#ifdef STM32G474xx
    while (ser_dev->Instance->ISR & USART_ISR_RXNE)
        uint8_t t = ser_dev->Instance->RDR;
#elif STM32F1
    while (ser_dev->Instance->SR & USART_SR_RXNE)
        uint8_t t = ser_dev->Instance->DR;
#else
    while (port->read() >= 0)
        ;
#endif
    u8lastRec = u8BufferSize = 0;
    u16InCnt = u16OutCnt = u16errCnt = 0;
}

/**
 * @brief
 * Method to write a new slave ID address
 *
 * @param 	u8id	new slave address between 1 and 247
 * @ingroup setup
 */
void Modbus::setID(uint8_t u8id)
{
    if ((u8id != 0) && (u8id <= 247))
    {
        this->u8id = u8id;
    }
}

/**
 * @brief
 * Method to read current slave ID address
 *
 * @return u8id	current slave address between 1 and 247
 * @ingroup setup
 */
uint8_t Modbus::getID()
{
    return this->u8id;
}

/**
 * @brief
 * Method to write the overtime count for txend pin.
 * It waits until count reaches 0 after the transfer is done.
 * With this, you can extend the time between txempty and
 * the falling edge if needed.
 *
 * @param 	uint32_t	overtime count for txend pin
 * @ingroup setup
 */
void Modbus::setTxendPinOverTime(uint32_t u32overTime)
{
    this->u32overTime = u32overTime;
}

/**
 * @brief
 * Initialize time-out parameter
 *
 * Call once class has been instantiated, typically within setup().
 * The time-out timer is reset each time that there is a successful communication
 * between Master and Slave. It works for both.
 *
 * @param time-out value (ms)
 * @ingroup setup
 */
void Modbus::setTimeOut(uint16_t u16timeOut)
{
    this->u16timeOut = u16timeOut;
}

/**
 * @brief
 * Return communication Watchdog state.
 * It could be usefull to reset outputs if the watchdog is fired.
 *
 * @return TRUE if millis() > u32timeOut
 * @ingroup loop
 */
bool Modbus::getTimeOutState()
{
    return ((unsigned long)(millis() - u32timeOut) > (unsigned long)u16timeOut);
}

/**
 * @brief
 * Get input messages counter value
 * This can be useful to diagnose communication
 *
 * @return input messages counter
 * @ingroup buffer
 */
uint16_t Modbus::getInCnt()
{
    return u16InCnt;
}

/**
 * @brief
 * Get transmitted messages counter value
 * This can be useful to diagnose communication
 *
 * @return transmitted messages counter
 * @ingroup buffer
 */
uint16_t Modbus::getOutCnt()
{
    return u16OutCnt;
}

/**
 * @brief
 * Get errors counter value
 * This can be useful to diagnose communication
 *
 * @return errors counter
 * @ingroup buffer
 */
uint16_t Modbus::getErrCnt()
{
    return u16errCnt;
}

/**
 * Get modbus master state
 *
 * @return = 0 IDLE, = 1 WAITING FOR ANSWER
 * @ingroup buffer
 */
uint8_t Modbus::getState()
{
    return u8state;
}

/**
 * Get the last error in the protocol processor
 *
 * @returnreturn   NO_REPLY = 255      Time-out
 * @return   EXC_FUNC_CODE = 1   Function code not available
 * @return   EXC_ADDR_RANGE = 2  Address beyond available space for Modbus registers
 * @return   EXC_REGS_QUANT = 3  Coils or registers number beyond the available space
 * @ingroup buffer
 */
uint8_t Modbus::getLastError()
{
    return u8lastError;
}

/**
 * @brief
 * *** Only Modbus Master ***
 * Generate a query to an slave with a modbus_t telegram structure
 * The Master must be in COM_IDLE mode. After it, its state would be COM_WAITING.
 * This method has to be called only in loop() section.
 *
 * @see modbus_t
 * @param modbus_t  modbus telegram structure (id, fct, ...)
 * @ingroup loop
 * @todo finish function 15
 */
int8_t Modbus::query(modbus_t telegram, uint16_t *au16data)
{
    uint8_t u8regsno, u8bytesno;
    if (u8id != 0)
        return -2;
    if (u8state != COM_IDLE)
        return -1;

    if ((telegram.u8id == 0) || (telegram.u8id > 247))
        return -3;

    // au16regs = telegram.au16reg;
    au16regs = au16data;

    // telegram header
    au8Buffer[ID] = telegram.u8id;
    au8Buffer[FUNC] = telegram.u8fct;
    au8Buffer[ADD_HI] = highByte(telegram.u16RegAdd);
    au8Buffer[ADD_LO] = lowByte(telegram.u16RegAdd);

    switch (telegram.u8fct)
    {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
    case MB_FC_READ_REGISTERS:
    case MB_FC_READ_INPUT_REGISTER:
        au8Buffer[NB_HI] = highByte(telegram.u16CoilsNo);
        au8Buffer[NB_LO] = lowByte(telegram.u16CoilsNo);
        u8BufferSize = 6;
        break;
    case MB_FC_WRITE_COIL:
        au8Buffer[NB_HI] = ((au16regs[0] > 0) ? 0xff : 0);
        au8Buffer[NB_LO] = 0;
        u8BufferSize = 6;
        break;
    case MB_FC_WRITE_REGISTER:
        au8Buffer[NB_HI] = highByte(au16regs[0]);
        au8Buffer[NB_LO] = lowByte(au16regs[0]);
        u8BufferSize = 6;
        break;
    case MB_FC_DIAGNOSTIC:
        au8Buffer[NB_HI] = 0;
        au8Buffer[NB_LO] = 0;
        u8BufferSize = 6;
        break;
    case MB_FC_WRITE_MULTIPLE_COILS: // TODO: implement "sending coils"
        u8regsno = telegram.u16CoilsNo / 16;
        u8bytesno = u8regsno * 2;
        if ((telegram.u16CoilsNo % 16) != 0)
        {
            u8bytesno++;
            u8regsno++;
        }

        au8Buffer[NB_HI] = highByte(telegram.u16CoilsNo);
        au8Buffer[NB_LO] = lowByte(telegram.u16CoilsNo);
        au8Buffer[BYTE_CNT] = u8bytesno;
        u8BufferSize = 7;

        for (uint16_t i = 0; i < u8bytesno; i++)
        {
            if (i % 2)
            {
                au8Buffer[u8BufferSize] = lowByte(au16regs[i / 2]);
            }
            else
            {
                au8Buffer[u8BufferSize] = highByte(au16regs[i / 2]);
            }
            u8BufferSize++;
        }
        break;

    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        au8Buffer[NB_HI] = highByte(telegram.u16CoilsNo);
        au8Buffer[NB_LO] = lowByte(telegram.u16CoilsNo);
        au8Buffer[BYTE_CNT] = (uint8_t)(telegram.u16CoilsNo * 2);
        u8BufferSize = 7;

        for (uint16_t i = 0; i < telegram.u16CoilsNo; i++)
        {
            au8Buffer[u8BufferSize] = highByte(au16regs[i]);
            u8BufferSize++;
            au8Buffer[u8BufferSize] = lowByte(au16regs[i]);
            u8BufferSize++;
        }
        break;
    }

#ifdef Arduino_h
    sendTxBuffer();
#else
    sendTxBuffer_HAL();
#endif
    u8state = COM_WAITING;
    u8lastError = 0;
    return 0;
}

/**
 * @brief *** Only for Modbus Master ***
 * This method checks if there is any incoming answer if pending.
 * If there is no answer, it would change Master state to COM_IDLE.
 * This method must be called only at loop section.
 * Avoid any delay() function.
 *
 * Any incoming data would be redirected to au16regs pointer,
 * as defined in its modbus_t query telegram.
 *
 * @params	nothing
 * @return errors counter
 * @ingroup loop
 */
int8_t Modbus::poll()
{
    // check if there is any incoming frame
    uint8_t u8current;
#ifdef Arduino_h
    u8current = port->available();
// check if there is any incoming frame
#elif STM32F1
    u8current = ser_dev->Instance->SR & USART_SR_RXNE;
#elif STM32G474xx
    u8current = ser_dev->Instance->ISR & USART_ISR_RXNE;
#endif

    if ((unsigned long)(millis() - u32timeOut) > (unsigned long)u16timeOut)
    {
        u8state = COM_IDLE;
        u8lastError = NO_REPLY;
        u16errCnt++;
        return 0;
    }

    if (u8current == 0)
        return 0;
#ifdef Arduino_h
    // check T35 after frame end or still no frame end
    if (u8current != u8lastRec)
    {
        u8lastRec = u8current;
        u32time = millis();
        return 0;
    }
    if ((unsigned long)(millis() - u32time) < (unsigned long)T35)
        return 0;

    // transfer Serial buffer frame to auBuffer
    u8lastRec = 0;
#endif
    int8_t i8state = getRxBuffer();
    if (i8state < 6) // 7 was incorrect for functions 1 and 2 the smallest frame could be 6 bytes long
    {
        u8state = COM_IDLE;
        u16errCnt++;
        return i8state;
    }

    // validate message: id, CRC, FCT, exception
    uint8_t u8exception = validateAnswer();
    if (u8exception != 0)
    {
        u8state = COM_IDLE;
        return u8exception;
    }

    // process answer
    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
        // call get_FC1 to transfer the incoming message to au16regs buffer
        get_FC1();
        break;
    case MB_FC_READ_INPUT_REGISTER:
    case MB_FC_READ_REGISTERS:
        // call get_FC3 to transfer the incoming message to au16regs buffer
        get_FC3();
        break;
    case MB_FC_WRITE_COIL:
    case MB_FC_WRITE_REGISTER:
        au16regs[0] = word(au8Buffer[4], au8Buffer[4 + 1]);
    case MB_FC_WRITE_MULTIPLE_COILS:
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        // nothing to do
        break;
    default:
        break;
    }
    u8state = COM_IDLE;
    return u8BufferSize;
}

/**
 * @brief
 * *** Only for Modbus Slave ***
 * This method checks if there is any incoming query
 * Afterwards, it would shoot a validation routine plus a register query
 * Avoid any delay() function !!!!
 * After a successful frame between the Master and the Slave, the time-out timer is reset.
 *
 * @param *regs  register table for communication exchange
 * @param u8size  size of the register table
 * @return 0 if no query, 1..4 if communication error, >4 if correct query processed
 * @ingroup loop
 */
#ifdef Arduino_h
int8_t Modbus::poll(bool *DO, bool *DI, uint16_t *AI, uint16_t *AO, uint8_t DO_u8size, uint8_t DI_u8size, uint8_t AI_u8size, uint8_t AO_u8size)
{

    au16regs = AO;

    uint8_t u8current;

    // check if there is any incoming frame

    u8current = port->available();

    if (u8current == 0)
        return 0;

    // check T35 after frame end or still no frame end
    if (u8current != u8lastRec)
    {
        u8lastRec = u8current;
        u32time = millis();
        return 0;
    }
    if ((unsigned long)(millis() - u32time) < (unsigned long)T35)
        return 0;

    u8lastRec = 0;
    int8_t i8state = getRxBuffer();
    u8lastError = i8state;
    if (i8state < 7)
        return i8state;

    // check slave id
    if (au8Buffer[ID] != u8id)
        return 0;

    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_DISCRETE_INPUT:
        u8regsize = DI_u8size;
        break;
    case MB_FC_READ_INPUT_REGISTER:
        u8regsize = AI_u8size;
        break;
    case MB_FC_READ_COILS:
    case MB_FC_WRITE_COIL:
    case MB_FC_WRITE_MULTIPLE_COILS:
        u8regsize = DO_u8size;
        break;
    case MB_FC_READ_REGISTERS:
    case MB_FC_WRITE_REGISTER:
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        u8regsize = AO_u8size;
        break;
    default:
        break;
    }

    // validate message: CRC, FCT, address and size
    uint8_t u8exception = validateRequest();
    if (u8exception > 0)
    {
        if (u8exception != NO_REPLY)
        {
            buildException(u8exception);
            sendTxBuffer();
        }
        u8lastError = u8exception;
        return u8exception;
    }

    u32timeOut = millis();
    u8lastError = 0;

    // process message
    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_COILS:
        process_FC1(DO, DO_u8size);
        break;
    case MB_FC_READ_DISCRETE_INPUT:
        process_FC1(DI, DI_u8size);
        break;
    case MB_FC_READ_REGISTERS:
        process_FC3(AO, AO_u8size);
        break;
    case MB_FC_READ_INPUT_REGISTER:
        process_FC3(AI, AI_u8size);
        break;
    case MB_FC_WRITE_COIL:
        process_FC5(DO, DO_u8size);
        break;
    case MB_FC_WRITE_REGISTER:
        process_FC6(AO, AO_u8size);
        break;
    case MB_FC_WRITE_MULTIPLE_COILS:
        process_FC15(DO, DO_u8size);
        break;
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        process_FC16(AO, AO_u8size);
        break;
    default:
        break;
    }
    sendTxBuffer();
    return 0;
}
#endif
int8_t Modbus::poll_IRQ(bool *DO, bool *DI, uint16_t *AI, uint16_t *AO, uint8_t DO_u8size, uint8_t DI_u8size, uint8_t AI_u8size, uint8_t AO_u8size)
{
    // check if there is any incoming frame
    // if ((ser_dev->Instance->SR & USART_SR_RXNE) == 0)
    //     return 0;

    au16regs = AO;

    static uint8_t u8current; // счетчик принятых байтов

    if ((unsigned long)(millis() - u32time) > (unsigned long)T35) // между байтами не более T35, чтобы не допустить прием чужих
    {
        u8current = 0;
    }
    u32time = millis();
#ifdef __AVR__
    if (port == &Serial)
    {
#ifdef UDR0
        au8Buffer[u8current] = UDR0; // принимаем байт в массив
#endif
        ;
    }
#ifdef UDR1
    else if (port == &Serial1)
        au8Buffer[u8current] = UDR1; // принимаем байт в массив
#endif
#endif
    // check slave id
    if (au8Buffer[ID] != u8id)
        return 0;

    u8current++;

    if (u8current >= MAX_BUFFER)
    {
        u16errCnt++;
        u8current = 0;
        u8lastError = ERR_BUFF_OVERFLOW;
        return ERR_BUFF_OVERFLOW;
    }

    if (u8current < 8)
        return 0;

    if (((au8Buffer[FUNC] == 15) || (au8Buffer[FUNC] == 16)) && (u8current < (au8Buffer[BYTE_CNT] + 9)))
        return 0;

    u8BufferSize = u8current;
    u8current = 0;

    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_DISCRETE_INPUT:
        u8regsize = DI_u8size;
        break;
    case MB_FC_READ_INPUT_REGISTER:
        u8regsize = AI_u8size;
        break;
    case MB_FC_READ_COILS:
    case MB_FC_WRITE_COIL:
    case MB_FC_WRITE_MULTIPLE_COILS:
        u8regsize = DO_u8size;
        break;
    case MB_FC_READ_REGISTERS:
    case MB_FC_WRITE_REGISTER:
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        u8regsize = AO_u8size;
        break;
    default:
        break;
    }

    // validate message: CRC, FCT, address and size
    uint8_t u8exception = validateRequest();
    if (u8exception > 0)
    {
        if (u8exception != NO_REPLY)
        {
            buildException(u8exception);
            sendTxBuffer();
        }
        u8lastError = u8exception;
        return u8exception;
    }

    u32timeOut = millis();
    u8lastError = 0;

    // process message
    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_COILS:
        process_FC1(DO, DO_u8size);
        break;
    case MB_FC_READ_DISCRETE_INPUT:
        process_FC1(DI, DI_u8size);
        break;
    case MB_FC_READ_REGISTERS:
        process_FC3(AO, AO_u8size);
        break;
    case MB_FC_READ_INPUT_REGISTER:
        process_FC3(AI, AI_u8size);
        break;
    case MB_FC_WRITE_COIL:
        process_FC5(DO, DO_u8size);
        break;
    case MB_FC_WRITE_REGISTER:
        process_FC6(AO, AO_u8size);
        break;
    case MB_FC_WRITE_MULTIPLE_COILS:
        process_FC15(DO, DO_u8size);
        break;
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        process_FC16(AO, AO_u8size);
        break;
    default:
        break;
    }
    sendTxBuffer();
    return 0;
}

#ifndef __AVR__
int8_t Modbus::poll_IRQ_HAL(bool *DO, bool *DI, uint16_t *AI, uint16_t *AO, uint8_t DO_u8size, uint8_t DI_u8size, uint8_t AI_u8size, uint8_t AO_u8size)
{
// check if there is any incoming frame
#if defined(STM32F1xx) || defined(STM32F1)
    if ((ser_dev->Instance->SR & USART_SR_RXNE) == 0)
#elif STM32G474xx
    if ((ser_dev->Instance->ISR & USART_ISR_RXNE) == 0)
#endif
        return 0;

    au16regs = AO;

    static uint8_t u8current; // счетчик принятых байтов

    if ((unsigned long)(millis() - u32time) > (unsigned long)T35) // между байтами не более T35, чтобы не допустить прием чужих
    {
        u8current = 0;
    }
    u32time = millis();

#if defined(STM32F1xx) || defined(STM32F1)
    au8Buffer[u8current] = ser_dev->Instance->DR; // принимаем байт в массив
#elif STM32G474xx
    au8Buffer[u8current] = ser_dev->Instance->RDR; // принимаем байт в массив
#endif

    // check slave id
    if (au8Buffer[ID] != u8id)
        return 0;

    u8current++;

    if (u8current >= MAX_BUFFER)
    {
        u16errCnt++;
        u8current = 0;
        u8lastError = ERR_BUFF_OVERFLOW;
        return ERR_BUFF_OVERFLOW;
    }

    if (u8current < 8)
        return 0;

    if (((au8Buffer[FUNC] == 15) || (au8Buffer[FUNC] == 16)) && (u8current < (au8Buffer[BYTE_CNT] + 9)))
        return 0;

    u8BufferSize = u8current;
    u8current = 0;

    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_DISCRETE_INPUT:
        u8regsize = DI_u8size;
        break;
    case MB_FC_READ_INPUT_REGISTER:
        u8regsize = AI_u8size;
        break;
    case MB_FC_READ_COILS:
    case MB_FC_WRITE_COIL:
    case MB_FC_WRITE_MULTIPLE_COILS:
        u8regsize = DO_u8size;
        break;
    case MB_FC_READ_REGISTERS:
    case MB_FC_WRITE_REGISTER:
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        u8regsize = AO_u8size;
        break;
    default:
        break;
    }

    // validate message: CRC, FCT, address and size
    uint8_t u8exception = validateRequest();
    if (u8exception > 0)
    {
        if (u8exception != NO_REPLY)
        {
            buildException(u8exception);
            sendTxBuffer_HAL();
        }
        u8lastError = u8exception;
        return u8exception;
    }

    u32timeOut = millis();
    u8lastError = 0;

    // process message
    switch (au8Buffer[FUNC])
    {
    case MB_FC_READ_COILS:
        process_FC1(DO, DO_u8size);
        break;
    case MB_FC_READ_DISCRETE_INPUT:
        process_FC1(DI, DI_u8size);
        break;
    case MB_FC_READ_REGISTERS:
        process_FC3(AO, AO_u8size);
        break;
    case MB_FC_READ_INPUT_REGISTER:
        process_FC3(AI, AI_u8size);
        break;
    case MB_FC_WRITE_COIL:
        process_FC5(DO, DO_u8size);
        break;
    case MB_FC_WRITE_REGISTER:
        process_FC6(AO, AO_u8size);
        break;
    case MB_FC_DIAGNOSTIC:
        process_FC8();
        break;
    case MB_FC_WRITE_MULTIPLE_COILS:
        process_FC15(DO, DO_u8size);
        break;
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        process_FC16(AO, AO_u8size);
        break;
    default:
        break;
    }
    sendTxBuffer_HAL();
    return 0;
}
#endif

/* _____PRIVATE FUNCTIONS_____________________________________________________ */

/**
 * @brief
 * This method moves Serial buffer data to the Modbus au8Buffer.
 *
 * @return buffer size if OK, ERR_BUFF_OVERFLOW if u8BufferSize >= MAX_BUFFER
 * @ingroup buffer
 */
#ifdef Arduino_h
int8_t Modbus::getRxBuffer()
{
    bool bBuffOverflow = false;

    if (u8txenpin > 1)
        digitalWrite(u8txenpin, LOW);

    u8BufferSize = 0;

    while (port->available())
    {
        au8Buffer[u8BufferSize] = port->read();
        u8BufferSize++;

        if (u8BufferSize >= MAX_BUFFER)
            bBuffOverflow = true;
    }

    u16InCnt++;

    if (bBuffOverflow)
    {
        u16errCnt++;
        return ERR_BUFF_OVERFLOW;
    }
    return u8BufferSize;
}
#else

int8_t Modbus::getRxBuffer()
{
    bool bBuffOverflow = false;

    u8BufferSize = 0;

#if defined(STM32F1xx) || defined(STM32F1)
    while (ser_dev->Instance->SR & USART_SR_RXNE)
    {
#elif STM32G474xx
    while (ser_dev->Instance->ISR & USART_ISR_RXNE)
    {
#endif
#if defined(STM32F1xx) || defined(STM32F1)
        au8Buffer[u8BufferSize] = ser_dev->Instance->DR; // принимаем байт в массив
#elif STM32G474xx
        au8Buffer[u8BufferSize] = ser_dev->Instance->RDR; // принимаем байт в массив
#endif
        if (au8Buffer[ID] != u8id)
            u8BufferSize++;
        u32time = millis();

        if (u8BufferSize >= MAX_BUFFER)
            bBuffOverflow = true;
#if defined(STM32F1xx) || defined(STM32F1)
        while ((ser_dev->Instance->SR & USART_SR_RXNE) == 0)
        {
#elif STM32G474xx
        while ((ser_dev->Instance->ISR & USART_ISR_RXNE) == 0)
        {
#endif
            if ((unsigned long)(millis() - u32time) > (unsigned long)T35)
                return u8BufferSize;
        }
    }

    u16InCnt++;

    if (bBuffOverflow)
    {
        u16errCnt++;
        return ERR_BUFF_OVERFLOW;
    }
    return u8BufferSize;
}
#endif

/**
 * @brief
 * This method transmits au8Buffer to Serial line.
 * Only if u8txenpin != 0, there is a flow handling in order to keep
 * the RS485 transceiver in output state as long as the message is being sent.
 * This is done with UCSRxA register.
 * The CRC is appended to the buffer before starting to send it.
 *
 * @param nothing
 * @return nothing
 * @ingroup buffer
 */
#ifdef Arduino_h
void Modbus::sendTxBuffer()
{
    // append CRC to message
    uint16_t u16crc = calcCRC(u8BufferSize);
    au8Buffer[u8BufferSize] = u16crc >> 8;
    u8BufferSize++;
    au8Buffer[u8BufferSize] = u16crc & 0x00ff;
    u8BufferSize++;

    if (u8txenpin > 1)
    {
        // set RS485 transceiver to transmit mode
        digitalWrite(u8txenpin, HIGH);
    }

    // transfer buffer to serial line
    port->write(au8Buffer, u8BufferSize);

    if (u8txenpin > 1)
    {
        // must wait transmission end before changing pin state
        // soft serial does not need it since it is blocking
        // ...but the implementation in SoftwareSerial does nothing
        // anyway, so no harm in calling it.
        port->flush();
        // return RS485 transceiver to receive mode
        volatile uint32_t u32overTimeCountDown = u32overTime;
        while (u32overTimeCountDown-- > 0)
            ;
        digitalWrite(u8txenpin, LOW);
    }
    while (port->read() >= 0)
        ;

    u8BufferSize = 0;

    // set time-out for master
    u32timeOut = millis();

    // increase message counter
    u16OutCnt++;
}
#endif

#ifndef __AVR__
void Modbus::sendTxBuffer_HAL()
{
    // append CRC to message
    uint16_t u16crc = calcCRC(u8BufferSize);
    au8Buffer[u8BufferSize] = u16crc >> 8;
    u8BufferSize++;
    au8Buffer[u8BufferSize] = u16crc & 0x00ff;
    u8BufferSize++;

    if (u8txenpin >= 1)
    {
// set RS485 transceiver to transmit mode
#if defined(STM32F1xx) || defined(STM32F1)
        HAL_GPIO_WritePin(RS_DIR_GPIO_Port, RS_DIR_Pin, GPIO_PIN_SET);
#endif
    }

    // transfer buffer to serial line
    for (uint8_t i = 0; i < u8BufferSize; i++)
    {
#if defined(STM32F1xx) || defined(STM32F1)
        while ((ser_dev->Instance->SR & USART_SR_TXE) == 0)
            ;                                 // ждем опустошения буфера
        ser_dev->Instance->DR = au8Buffer[i]; // отправляем байт
#elif STM32G474xx
        while ((ser_dev->Instance->ISR & USART_ISR_TXE) == 0)
            ;                                  // ждем опустошения буфера
        ser_dev->Instance->TDR = au8Buffer[i]; // отправляем байт
#endif
        // SendArr[i] = 0;    // сразу же чистим переменную
    }
#if defined(STM32F1xx) || defined(STM32F1)
    while ((ser_dev->Instance->SR & USART_SR_TXE) == 0)
        ; // ждем опустошения буфера
#elif STM32G474xx
    while ((ser_dev->Instance->ISR & USART_ISR_TXE) == 0)
        ; // ждем опустошения буфера
#endif

    if (u8txenpin >= 1)
    {
        // must wait transmission end before changing pin state
        // soft serial does not need it since it is blocking
        // ...but the implementation in SoftwareSerial does nothing
        // anyway, so no harm in calling it.

        // return RS485 transceiver to receive mode
        volatile uint32_t u32overTimeCountDown = u32overTime;
        while (u32overTimeCountDown-- > 0)
            ;
#if defined(STM32F1xx) || defined(STM32F1)
        HAL_GPIO_WritePin(RS_DIR_GPIO_Port, RS_DIR_Pin, GPIO_PIN_RESET);
#endif
    }
#if defined(STM32F1xx) || defined(STM32F1)
    while ((ser_dev->Instance->SR & USART_SR_RXNE) == 1)
        uint8_t clear = ser_dev->Instance->DR;
#elif STM32G474xx
    while ((ser_dev->Instance->ISR & USART_ISR_RXNE) == 1)
        uint8_t clear = ser_dev->Instance->RDR;
#endif

    u8BufferSize = 0;

    // set time-out for master
    u32timeOut = millis();

    // increase message counter
    u16OutCnt++;
}
#endif

/**
 * @brief
 * This method calculates CRC
 *
 * @return uint16_t calculated CRC value for the message
 * @ingroup buffer
 */
uint16_t Modbus::calcCRC(uint8_t u8length)
{
    unsigned int temp, temp2, flag;
    temp = 0xFFFF;
    for (unsigned char i = 0; i < u8length; i++)
    {
        temp = temp ^ au8Buffer[i];
        for (unsigned char j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>= 1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;
}

/**
 * @brief
 * This method validates slave incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t Modbus::validateRequest()
{
    // check message crc vs calculated crc
    uint16_t u16MsgCRC =
        ((au8Buffer[u8BufferSize - 2] << 8) | au8Buffer[u8BufferSize - 1]); // combine the crc Low & High bytes
    if (calcCRC(u8BufferSize - 2) != u16MsgCRC)
    {
        u16errCnt++;
        return NO_REPLY;
    }

    // check fct code
    bool isSupported = false;
    for (uint8_t i = 0; i < sizeof(fctsupported); i++)
    {
        if (fctsupported[i] == au8Buffer[FUNC])
        {
            isSupported = 1;
            break;
        }
    }
    if (!isSupported)
    {
        u16errCnt++;
        return EXC_FUNC_CODE;
    }

    // check start address & nb range
    uint16_t u16regs = 0;
    uint8_t u8regs;
    switch (au8Buffer[FUNC])
    {
    // case MB_FC_READ_COILS:
    // case MB_FC_READ_DISCRETE_INPUT:
    // case MB_FC_WRITE_MULTIPLE_COILS:
    //     u16regs = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]) / 16;
    //     u16regs += word(au8Buffer[NB_HI], au8Buffer[NB_LO]) / 16;
    //     u8regs = (uint8_t)u16regs;
    //     if (u8regs > u8regsize)
    //         return EXC_ADDR_RANGE;
    //     break;
    case MB_FC_WRITE_COIL:
        u16regs = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]) / 16;
        u8regs = (uint8_t)u16regs;
        if (u8regs > u8regsize)
            return EXC_ADDR_RANGE;
        break;
    case MB_FC_WRITE_REGISTER:
        u16regs = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
        u8regs = (uint8_t)u16regs;
        if (u8regs > u8regsize)
            return EXC_ADDR_RANGE;
        break;
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
    case MB_FC_WRITE_MULTIPLE_COILS:
    case MB_FC_READ_REGISTERS:
    case MB_FC_READ_INPUT_REGISTER:
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        u16regs = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
        u16regs += word(au8Buffer[NB_HI], au8Buffer[NB_LO]);
        u8regs = (uint8_t)u16regs;
        if (u8regs > u8regsize)
            return EXC_ADDR_RANGE;
        break;
    case MB_FC_DIAGNOSTIC:
        break;
    }
    return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method validates master incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t Modbus::validateAnswer()
{
    // check message crc vs calculated crc
    uint16_t u16MsgCRC =
        ((au8Buffer[u8BufferSize - 2] << 8) | au8Buffer[u8BufferSize - 1]); // combine the crc Low & High bytes
    if (calcCRC(u8BufferSize - 2) != u16MsgCRC)
    {
        u16errCnt++;
        return NO_REPLY;
    }

    // check exception
    if ((au8Buffer[FUNC] & 0x80) != 0)
    {
        u16errCnt++;
        return ERR_EXCEPTION;
    }

    // check fct code
    bool isSupported = false;
    for (uint8_t i = 0; i < sizeof(fctsupported); i++)
    {
        if (fctsupported[i] == au8Buffer[FUNC])
        {
            isSupported = 1;
            break;
        }
    }
    if (!isSupported)
    {
        u16errCnt++;
        return EXC_FUNC_CODE;
    }

    return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method builds an exception message
 *
 * @ingroup buffer
 */
void Modbus::buildException(uint8_t u8exception)
{
    uint8_t u8func = au8Buffer[FUNC]; // get the original FUNC code

    au8Buffer[ID] = u8id;
    au8Buffer[FUNC] = u8func + 0x80;
    au8Buffer[2] = u8exception;
    u8BufferSize = EXCEPTION_SIZE;
}

/**
 * This method processes functions 1 & 2 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 * TODO: finish its implementation
 */
void Modbus::get_FC1()
{
    uint8_t u8byte, i;
    u8byte = 3;
    for (i = 0; i < au8Buffer[2]; i++)
    {

        if (i % 2)
        {
            au16regs[i / 2] = word(au8Buffer[i + u8byte], lowByte(au16regs[i / 2]));
        }
        else
        {

            au16regs[i / 2] = au8Buffer[i + u8byte];
        }
    }
}

/**
 * This method processes functions 3 & 4 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 */
void Modbus::get_FC3()
{
    uint8_t u8byte, i;
    u8byte = 3;

    for (i = 0; i < au8Buffer[2] / 2; i++)
    {
        au16regs[i] = word(au8Buffer[u8byte], au8Buffer[u8byte + 1]);
        u8byte += 2;
    }
}

/**
 * @brief
 * This method processes functions 1 & 2
 * This method reads a bit array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC1(bool *regs, uint8_t /*u8size*/)
{
    uint8_t u8bytesno, u8bitsno;
    uint8_t u8CopyBufferSize;

    // get the first and last coil from the message
    uint8_t u16StartCoil = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
    uint16_t u16Coilno = word(au8Buffer[NB_HI], au8Buffer[NB_LO]);

    // put the number of bytes in the outcoming message
    u8bytesno = (uint8_t)(u16Coilno / 8);
    if (u16Coilno % 8 != 0)
        u8bytesno++;
    au8Buffer[ADD_HI] = u8bytesno;
    u8BufferSize = ADD_LO;
    au8Buffer[u8BufferSize + u8bytesno - 1] = 0;

    // read each coil from the register map and put its value inside the outcoming message
    u8bitsno = 0;

    for (size_t i = 0; i < u16Coilno; i++)
    {
        au8Buffer[u8BufferSize] |= uint8_t(regs[u16StartCoil + i] << u8bitsno);

        u8bitsno++;

        if (u8bitsno > 7)
        {
            u8bitsno = 0;
            u8BufferSize++;
        }
    }

    // send outcoming message
    if (u16Coilno % 8 != 0)
        u8BufferSize++;
    u8CopyBufferSize = u8BufferSize + 2;

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes functions 3 & 4
 * This method reads a word array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC3(uint16_t *regs, uint8_t /*u8size*/)
{

    uint8_t u8StartAdd = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
    uint8_t u8regsno = word(au8Buffer[NB_HI], au8Buffer[NB_LO]);
    uint8_t u8CopyBufferSize;
    uint8_t i;

    au8Buffer[2] = u8regsno * 2;
    u8BufferSize = 3;

    for (i = u8StartAdd; i < u8StartAdd + u8regsno; i++)
    {
        au8Buffer[u8BufferSize] = highByte(regs[i]);
        u8BufferSize++;
        au8Buffer[u8BufferSize] = lowByte(regs[i]);
        u8BufferSize++;
    }
    u8CopyBufferSize = u8BufferSize + 2;

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 5
 * This method writes a value assigned by the master to a single bit
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC5(bool *regs, uint8_t /*u8size*/)
{
    uint8_t u8CopyBufferSize;
    uint8_t u16coil = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);

    // write to coil
    if (au8Buffer[NB_HI] == 0xff)
        regs[u16coil] = true;
    else
        regs[u16coil] = false;

    // send answer to master
    u8BufferSize = 6;
    u8CopyBufferSize = u8BufferSize + 2;

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 6
 * This method writes a value assigned by the master to a single word
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC6(uint16_t *regs, uint8_t /*u8size*/)
{

    uint8_t u8add = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
    uint8_t u8CopyBufferSize;
    uint16_t u16val = word(au8Buffer[NB_HI], au8Buffer[NB_LO]);

    regs[u8add] = u16val;

    // keep the same header
    u8BufferSize = RESPONSE_SIZE;

    u8CopyBufferSize = u8BufferSize + 2;

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 8
 * This method diagnostic
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC8()
{

    uint8_t u8add = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
    // uint16_t u16val = word(au8Buffer[NB_HI], au8Buffer[NB_LO]);

    if (u8add == 1)
    {
        restart();
    }

    return 0;
}

/**
 * @brief
 * This method processes function 15
 * This method writes a bit array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC15(bool *regs, uint8_t /*u8size*/)
{
    uint8_t u8frameByte, u8bitsno;
    uint8_t u8CopyBufferSize;

    // get the first and last coil from the message
    uint8_t u16StartCoil = word(au8Buffer[ADD_HI], au8Buffer[ADD_LO]);
    uint16_t u16Coilno = word(au8Buffer[NB_HI], au8Buffer[NB_LO]);

    // read each coil from the register map and put its value inside the outcoming message
    u8bitsno = 0;
    u8frameByte = 7;

    for (size_t i = 0; i < u16Coilno; i++)
    {
        regs[u16StartCoil + i] = uint8_t(au8Buffer[u8frameByte] & (u8bitsno + 1)) >> u8bitsno;
        u8bitsno++;

        if (u8bitsno > 7)
        {
            u8bitsno = 0;
            u8frameByte++;
        }
    }

    // send outcoming message
    // it's just a copy of the incomping frame until 6th byte
    u8BufferSize = 6;
    u8CopyBufferSize = u8BufferSize + 2;

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 16
 * This method writes a word array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC16(uint16_t *regs, uint8_t /*u8size*/)
{
    uint8_t u8StartAdd = au8Buffer[ADD_HI] << 8 | au8Buffer[ADD_LO];
    uint8_t u8regsno = au8Buffer[NB_HI] << 8 | au8Buffer[NB_LO];
    uint8_t u8CopyBufferSize;
    uint16_t temp;

    // build header
    au8Buffer[NB_HI] = 0;
    au8Buffer[NB_LO] = u8regsno;
    u8BufferSize = RESPONSE_SIZE;

    // write registers
    for (uint8_t i = 0; i < u8regsno; i++)
    {
        temp = word(
            au8Buffer[(BYTE_CNT + 1) + i * 2],
            au8Buffer[(BYTE_CNT + 2) + i * 2]);

        regs[u8StartAdd + i] = temp;
    }
    u8CopyBufferSize = u8BufferSize + 2;

    return u8CopyBufferSize;
}