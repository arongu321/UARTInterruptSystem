/*
   UART Interrupt System
 * Created By: Aron Gu, January 2024
 */

#include "stdio.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xtime_l.h"

// UART driver header file
#include "uart_driver.h"

// Devices
#define SSD_DEVICE_ID   XPAR_AXI_SSD_DEVICE_ID
#define BTN_DEVICE_ID	XPAR_AXI_GPIO_0_DEVICE_ID

// Device channels
#define SSD_CHANNEL		1
#define BTN_CHANNEL		1

// Other Useful Macros
#define BTN0 1
#define BTN1 2
#define BTN2 4
#define BTN3 8
#define CHAR_ESC				0x23	// '#' character is used as termination sequence
#define CHAR_CARRIAGE_RETURN	0x0D	// '\r' character is used in the termination sequence
#define SEQUENCE_LENGTH 3 				//Rolling buffer sequence length


// Device declaration
XGpio SSDInst, btnInst;
u8 rollingBuffer[SEQUENCE_LENGTH] = {0};

// Function prototypes
void vBufferReceiveTask(void *p);
void vBufferSendTask(void *p);
void printString(char countMessage[]);
void printNumber(char number[]);
u8 checkBufferSequence(char* sequence);
void updateRollingBuffer(u8 receivedByte);
u32 sevenSegDecode(int digit, u8 cathode);

TaskHandle_t task_receiveuarthandle = NULL;
TaskHandle_t task_transmituarthandle = NULL;
int byteCount;	// to count the number of bytes received over the UART


int main()
{
	int status;

	// SSD

	status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}

	// Buttons
	status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}

	/* Device data direction: 0 for output 1 for input */
	XGpio_SetDataDirection(&SSDInst, SSD_CHANNEL, 0x00);
	XGpio_SetDataDirection(&btnInst, BTN_CHANNEL, 0x0F);


	xTaskCreate( vBufferReceiveTask
			   , "uart_receive_task"
			   , 1024
			   , (void*)0
			   , tskIDLE_PRIORITY
			   , &task_receiveuarthandle
			   );

	xTaskCreate( vBufferSendTask
			   , "uart_transmit_task"
			   , 1024
			   , (void*)0
			   , tskIDLE_PRIORITY
			   , &task_transmituarthandle
			   );


	xTxQueue = xQueueCreate( SIZE_OF_QUEUE, sizeof( u8 ) );
	xRxQueue  = xQueueCreate( SIZE_OF_QUEUE, sizeof( u8 ));

	countRxIrq = 0;
	countTxIrq = 0;
	byteCount = 0;

	status = initializeUART();

	if (status != XST_SUCCESS){
		xil_printf("UART Initialization failed\n");
	}

	configASSERT(xTxQueue);
	configASSERT(xRxQueue);

	xil_printf(
	    "\n====== App Ready ======\n"
	    "Instructions:\n"
	    "- Send data via serial terminal. Press Enter to swap case of letters.\n"
	    "  (Numbers/symbols unchanged).\n"
	    "- To view interrupt count, type: '\\r#\\r'\n"
	    "- To reset interrupt count, type: '\\r%\\r'\n"
	    "- BTN0: Display Rx interrupt count on SSD.\n"
	    "- BTN1: Display Tx interrupt count on SSD.\n"
		"- BTN2: Display byte count on SSD.\n"
		"- BTN3: Reset interrupt and byte count.\n"
	    "========================\n\n"
	);

	vTaskStartScheduler();

	while(1);

	return 0;

}

void vBufferReceiveTask(void *p)
{
	int status;
	u8 pcString, cathode=0;
	char formattedChar ;
	u32 ssd_value = 1; // Value to be displayed on the SSD
	unsigned int buttonVal=0;

	status = setupInterruptSystem( &InterruptController
								 , &UART
								 , UART_INT_IRQ_ID
								 );

	if (status != XST_SUCCESS){
		xil_printf("UART PS interrupt failed\n");
	}

	while(1){

		while(myReceiveData() == pdFALSE){
			buttonVal = XGpio_DiscreteRead(&btnInst, 1);
			if(buttonVal == BTN0){
				ssd_value = sevenSegDecode(countRxIrq, cathode);
			} else if(buttonVal == BTN1) {
				ssd_value = sevenSegDecode(countTxIrq, cathode);
			} else if(buttonVal == BTN2) {
				ssd_value = sevenSegDecode(byteCount, cathode);
			} else if(buttonVal == BTN3) {
				byteCount = 0;
				countRxIrq = 0;
				countTxIrq = 0;
				ssd_value = sevenSegDecode(88, cathode);
			} else {
				ssd_value = sevenSegDecode(0, cathode);
			}
			XGpio_DiscreteWrite(&SSDInst, SSD_CHANNEL, ssd_value);
			cathode = !cathode;
		}

/*************************** Enter your code here ****************************/
		// TODO 7: Call user defined receive function to return the received data.
		// Store value in "pcString" variable
		pcString = myReceiveByte();
		while(myTransmitFull() == pdTRUE);
		formattedChar  = (char) pcString;
		// Write the code to change the capitalization of the received byte
		if(formattedChar >= 'a' && formattedChar <= 'z'){
			formattedChar -= 32; // Convert to uppercase by subtracting 32 from its ASCII value
		}
		else if(formattedChar >= 'A' && formattedChar <= 'Z'){
			formattedChar += 32; // Convert to lowercase
		}
		// Increment the receive interrupt counter
		byteCount++;
		updateRollingBuffer(pcString);
		mySendByte(formattedChar );

		// Detect \r#\r sequence here.
		if( checkBufferSequence("\r#\r") ){
			taskYIELD(); //perform context switch
		}
		// detect \r%\r sequence here, if detected set byte and interrupt counters to 0
		// and print the message "Byte Count, and interrupt counters set to zero\n\n"
		if( checkBufferSequence("\r%\r") ){
			byteCount = 0;
			countRxIrq = 0;
			countTxIrq = 0;
			xil_printf("Byte Count, and interrupt counters set to zero\n\n");
		}

/*****************************************************************************/
	}
}


void vBufferSendTask(void *p)
{
	while(1){

		char countArray[10];
		char CountRxIrqArray[10];
		char CountTxIrqArray[10];

		//print the special messages for ending sequence
		sprintf(countArray,"%d", byteCount);
		sprintf(CountRxIrqArray,"%d", countRxIrq);
		sprintf(CountTxIrqArray,"%d", countTxIrq);
		printString("Byte count: ");
		printNumber(countArray);
		mySendByte(CHAR_CARRIAGE_RETURN);
		printString("Rx interrupts: ");
		printNumber(CountRxIrqArray);
		mySendByte(CHAR_CARRIAGE_RETURN);
		printString("Tx interrupts: ");
		printNumber(CountTxIrqArray);
		taskYIELD(); // Force context switch
	}
}


//print the provided number using driver functions
void printNumber(char number[])
{
	for (int i = 0; i < 10; i++){
		if (number[i] >= '0' && number[i] <= '9'){
			while(myTransmitFull() == pdTRUE){
				vTaskDelay(1);
			}
			mySendByte(number[i]);
		}
	}
}
/*----------------------------------------------------------------------------*/

//print the provided string using driver functions
void printString(char countMessage[])
{
	for (int i = 0; countMessage[i] != '\0'; i++){
		while(myTransmitFull() == pdTRUE){
			vTaskDelay(1);
		}
		mySendByte(countMessage[i]);
	}
}


u8 checkBufferSequence(char* sequence)
{
	if ( rollingBuffer[0] == sequence[0]
	  && rollingBuffer[1] == sequence[1]
	  && rollingBuffer[2] == sequence[2]
	   )
	{
		return 1;
	}
	return 0;
}


void updateRollingBuffer(u8 receivedByte)
{
	for (int i = 0; i < SEQUENCE_LENGTH - 1; i++){
		rollingBuffer[i] = rollingBuffer[i+1];
    }

    rollingBuffer[SEQUENCE_LENGTH - 1] = receivedByte;
}


// This function translates int values to their binary representation
u32 sevenSegDecode(int countValue, u8 cathode)
{
    u32 result;
    int digit;

    // Convert countValue to two decimal digits
    if (cathode == 0) {
        // LSD: the least significant digit
        digit = countValue % 10;
    } else {
        // MSD: the most significant digit
        digit = (countValue / 10) % 10;
    }

    // Map the digit to the corresponding 7-segment display encoding
    switch(digit){
        case 0: result = 0b00111111; break; // 0
        case 1: result = 0b00110000; break; // 1
        case 2: result = 0b01011011; break; // 2
        case 3: result = 0b01111001; break; // 3
        case 4: result = 0b01110100; break; // 4
        case 5: result = 0b01101101; break; // 5
        case 6: result = 0b01101111; break; // 6
        case 7: result = 0b00111000; break; // 7
        case 8: result = 0b01111111; break; // 8
        case 9: result = 0b01111100; break; // 9
        default: result = 0b00000000; break; // Undefined, all segments are OFF
    }

    // The cathode logic remains unchanged
    if (cathode == 1) {
        return result;
    } else {
        return result | 0b10000000;
    }
}
