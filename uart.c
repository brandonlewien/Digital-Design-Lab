#include "uart.h"
#include "ldma.h"

volatile bool ready_to_TX;
extern char receive_buffer[RECEIVE_BUFFER_SIZE];
volatile bool isCelsius = true;

/******************************************************************************
 * @brief Initialize LEUART0
 * @param none
 * @return none
 *****************************************************************************/
void uart_init(void) {
    LEUART_Init_TypeDef UART_Init_Struct;
    UART_Init_Struct.enable   = UART_DISABLE;
    UART_Init_Struct.refFreq  = UART_REF_FREQ;
    UART_Init_Struct.baudrate = UART_BAUD_RATE;
    UART_Init_Struct.databits = UART_DATA_BITS;
    UART_Init_Struct.parity   = UART_PARITY;
    UART_Init_Struct.stopbits = UART_STOP_BITS;
    LEUART0->CTRL |= LEUART_CTRL_TXDMAWU;                           // DMA Wakeup

    LEUART0_Interrupt_Disable();
    LEUART_Reset(LEUART0);

    LEUART_Init(LEUART0, &UART_Init_Struct);

    LEUART0->ROUTELOC0 = LEUART_ROUTELOC0_RXLOC_LOC18               // Route UART pins
                       | LEUART_ROUTELOC0_TXLOC_LOC18;
    LEUART0->ROUTEPEN  = LEUART_ROUTEPEN_RXPEN
                       | LEUART_ROUTEPEN_TXPEN;

    LEUART0->CMD = LEUART_CMD_RXBLOCKEN;                            // set RX buffer to discard incoming frames (clear it only after '?' has been RXed)
    LEUART0->CTRL |= LEUART_CTRL_SFUBRX;
    LEUART0->STARTFRAME = QUESTION_MARK;                            // set start frame to ascii question mark
    LEUART0->SIGFRAME = HASHTAG;                                    // set signal frame to ascii hashtag
    LEUART0->CTRL &= ~LEUART_CTRL_LOOPBK;                           // disable loopback
    GPIO_PinModeSet(TX_PORT, TX_PIN, gpioModePushPull, UART_ON);    // enable UART pins
    GPIO_PinModeSet(RX_PORT, RX_PIN, gpioModePushPull, UART_ON);

    ready_to_TX = 0;                                                // initialize transmit ready flag to 0

    LEUART_Enable(LEUART0, leuartEnable);
}
/******************************************************************************
 * @brief Send a single byte via uart
 * @param data = a single byte
 * @return ready_t0_TX gets reset to 0
 *****************************************************************************/
void UART_send_byte(uint8_t data) {
    LEUART0->IEN |= LEUART_IEN_TXBL;                                // enable TXBL interrupt (only want this enabled when we want to transmit data)
    while(!ready_to_TX){
        Enter_Sleep();                                              // sleep while waiting for space to be available in the transmit buffer
    }
    LEUART0->TXDATA = data;                                         // send data
    ready_to_TX = 0;                                                // reset flag to 0
}
/******************************************************************************
 * @brief Send a multibyte array via uart of specific length
 * @param Input array = data, length = array size
 * @return none
 *****************************************************************************/
void UART_send_n(char * data, uint32_t length) {
    for(int i = 0; i < length; i++) {
        UART_send_byte(data[i]);                                    // Loop through data and send
    }
}
/******************************************************************************
 * @brief Convert a float input to ascii
 * @param A float number to be converted
 * @return none
 *****************************************************************************/
void UART_ftoa_send(float number) {                                 // convert float to ascii value and send via UART
    int16_t integer = (int16_t)number;
    uint16_t decimal;

    if(integer < 0) {                                               // test if negative
        UART_send_byte(NEGATIVE_SIGN);                              // send negative sign
        decimal = (((-1) * (number - integer)) * 10);               // find decimal value
        integer = -1 * integer;                                     // make value positive for all following operations
    }
    else {
        UART_send_byte(POSITIVE_SIGN);                              // send positive sign
        decimal = ((number - integer) * 10);                        // find decimal values
    }
    if(((integer % 1000) / 100) != 0) {                             // hundreds place
        UART_send_byte((integer / 100) + ASCII_OFFSET);
    }
    else {
        UART_send_byte(0x20);                                       // if 0 value, send space instead
    }
    if((((integer % 100) / 10) != 0) || (((integer % 1000) / 100) != 0)) {  // tens place
        UART_send_byte(((integer % 100) / 10) + ASCII_OFFSET);
    }
    else {
        UART_send_byte(0x20);                                       // if 0 value, send space instead
    }
    if(((integer % 10) != 0) || (((integer % 1000) / 100) != 0) || (((integer % 100) / 10) != 0)) {     // ones place
        UART_send_byte((integer % 10) + ASCII_OFFSET);
    }
    else {
        UART_send_byte(SPACE);                                      // if 0 value, send space instead
    }

    UART_send_byte(DECIMAL_POINT);                                  // decimal point
    UART_send_byte(decimal + ASCII_OFFSET);                         // tenths place
}
/******************************************************************************
 * @brief Enable LEUART0 Interrupts
 * @param none
 * @return none
 *****************************************************************************/
void LEUART0_Interrupt_Enable(void) {
    LEUART0->IEN = 0;
    LEUART0->IEN = LEUART_IEN_SIGF;
    NVIC_EnableIRQ(LEUART0_IRQn);
}
/******************************************************************************
 * @brief Disable LEUART0 Interrupts
 * @param none
 * @return none
 *****************************************************************************/
void LEUART0_Interrupt_Disable(void) {
    LEUART0->IEN &= ~(LEUART_IEN_SIGF);
    NVIC_DisableIRQ(LEUART0_IRQn);
}
/******************************************************************************
 * @brief Switch between C to F depending on inputs. Handles random jibberish
 * @param Input buffer for decoding
 * @return isCelsius as a global if the user wants Celsius or Fahrenheit
 *****************************************************************************/
static void LEUART0_Receiver_Decoder(char * buffer) {
    for(int i = 0; i < RECEIVE_BUFFER_SIZE-1; ++i) {
        if ((buffer[i] == LOWER_D) || (buffer[i] == UPPER_D)) {
            if ((buffer[i+1] == LOWER_C) || (buffer[i+1] == UPPER_C)) {
                isCelsius = true;
                break;
            }
            else if ((buffer[i+1] == LOWER_F) || (buffer[i+1] == UPPER_F)) {
                isCelsius = false;
                break;
            }
        }
    }
}
/******************************************************************************
 * @brief IRQ Handler for LEUART0
 * @param none
 * @return receive_buffer gets cleared
 *****************************************************************************/
void LEUART0_IRQHandler(void) {
    uint32_t status;
    status = LEUART0->IF & LEUART0->IEN;
    if(status & LEUART_IF_TXBL) {
        ready_to_TX = 1;                                        // set ready to TX flag
        LEUART0->IEN &= ~LEUART_IEN_TXBL;                       // disable TXBL interrupt (only want this enabled when we want to transmit data)
    }
    if (status & LEUART_IF_SIGF) {
        LEUART0->CMD = LEUART_CMD_RXBLOCKEN;                    // enable block on RX UART buffer
        LEUART0_Receiver_Decoder(receive_buffer);               // Process data received
        LEUART0->IFC = LEUART_IFC_SIGF;
    }
    for (int i = 0; i < RECEIVE_BUFFER_SIZE; i++) {             // clear buffer
        receive_buffer[i] = 0;
    }
    if (status & LEUART_IF_TXC) {                               // if this statement is entered, we know that the last byte of DMA is complete
        LEUART0->IFC = LEUART_IFC_TXC;                          // clear TXC flag
        LEUART0->IEN &= ~LEUART_IEN_TXC;                        // disable TXC after last byte of DMA transfer has been signaled
        Sleep_UnBlock_Mode(LEUART_EM_BLOCK);
    }
}
