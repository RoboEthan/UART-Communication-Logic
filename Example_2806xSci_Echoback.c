/* Baud rate = 9600
 * Data = 8
 * Stop bit = 1
 * Parity = none
 *  Newline at CR+LF
 *  Make sure terminating character is sent with data (marks end)
*/

// Included Files
#include "DSP28x_Project.h"     // Device Headerfile and Examples Include File
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define PWM_FREQUENCY_MIN 687
#define PWM_FREQUENCY_MAX 10000
#define SIN_FREQUENCY_MIN 0
#define SIN_FREQUENCY_MAX 150
#define MODULATION_DEPTH_MIN 0
#define MODULATION_DEPTH_MAX 100
#define CLKRATE 90.0*1000000.0
#define MAX_BUFFER_SIZE 256
#define MAX_MSG_SIZE 100


typedef struct {
    float pwm_frequency;
    float sin_frequency;
    float modulation_depth;
    float offset;
    float angle_1;
    float angle_2;
    float angle_3;
    Uint32 epwmTimerTBPRD;
} EPwmParams;


// Function Prototypes
void scia_echoback_init(void);
void scia_fifo_init(void);
void scia_xmit(int a);
void scia_msg(char *msg);
int populate_variable(const char *arr, float *var, float min, float max);

// Globals
Uint16 LoopCount;
Uint16 ErrorCount;




// Main
void main(void)
{
    //Struct to hold parameters
     EPwmParams epwmParams = {
        .pwm_frequency = 5000,     //frequency of pwm DO NOT GO BELOW 687Hz, counter wont work properly 65535 < 90*10^6 / (687*2)
        .sin_frequency = 60,       //sin frequency 0-150Hz
        .modulation_depth = 1.0,   //modulation depth between 0 and 1
        .offset = 0.0,             //make sure offset is between +-(1-MODULATION_DEPTH)/2
        .angle_1 = 0.0,            //Phase shift angle in degree
        .angle_2 = 120.0,          //Phase shift angle in degree
        .angle_3 = 240.0,          //Phase shift angle in degree
        .epwmTimerTBPRD = 0
    };

    Uint16 ReceivedChar;
    char buffer[MAX_BUFFER_SIZE] = { '\0' };
    Uint16 bufferIndex = 0;
    char *msg;

    // Step 1. Initialize System Control:
    // PLL, WatchDog, enable Peripheral Clocks
    // This example function is found in the F2806x_SysCtrl.c file.
    InitSysCtrl();

    // For this example, only init the pins for the SCI-A port.
    // This function is found in the F2806x_Sci.c file.
    InitSciaGpio();

    // Step 3. Clear all interrupts and initialize PIE vector table:
    // Disable CPU interrupts
    DINT;

    // Initialize PIE control registers to their default state.
    // The default state is all PIE interrupts disabled and flags
    // are cleared.
    // This function is found in the F2806x_PieCtrl.c file.
    InitPieCtrl();

    // Disable CPU interrupts and clear all CPU interrupt flags
    IER = 0x0000;
    IFR = 0x0000;

    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    // This will populate the entire table, even if the interrupt
    // is not used in this example.  This is useful for debug purposes.
    // The shell ISR routines are found in F2806x_DefaultIsr.c.
    // This function is found in F2806x_PieVect.c.
    InitPieVectTable();

    // Step 5. User specific code
    LoopCount = 0;
    ErrorCount = 0;

    scia_fifo_init();     // Initialize the SCI FIFO
    scia_echoback_init();  // Initalize SCI for echoback

    msg = "\r\nEnter a string in the format 'P 2000,S 60,M 1.0,O 0.0,A1 0.0,A2 120.0,A3 240.0'\0";
    scia_msg(msg);

    for (;;)
    {
        while (SciaRegs.SCIFFRX.bit.RXFFST < 1)
        {
            // wait for a character
        }

        // Get character
        ReceivedChar = SciaRegs.SCIRXBUF.all;

        // Check if it's a terminating character
        if (ReceivedChar == '\0')
        {
            // Echo the buffer back
            msg = "\r\nYou sent: ";
            scia_msg(msg);
            buffer[bufferIndex] = '\0'; // Null-terminate the buffer
            scia_msg(buffer);

            //TODO: make this a function in the future

            //process the buffer
            int i = 0, space = 0;
            while (buffer[i] != '\0') //loops until null character is found   TODO:change to find length and loop a discrete amount
            {
                //checks for multiple parameters
                if(buffer[i] == ',')
                    i++;

                //checks for extra spaces
                while(buffer[i] == ' ')
                {
                    i++;
                }

                //checks for space sandwiched between letter value TODO: (does not work if F___231 more than one space here)
                if (buffer[i+1] == ' ' && buffer[i+2] < ':' )
                    space = 1;
/////////CHANGE THE DECIMALS maybe? atoi(to_string(epwmParams.pwm_frequency)
                switch (buffer[i])
                {
                case 'P':
                    populate_variable(&(buffer[i]), &epwmParams.pwm_frequency, 687, 5000);
                    i += (int)log10((double)epwmParams.pwm_frequency) + 2 + space;
                    space = 0;
                    break;
                case 'S':
                    populate_variable(&(buffer[i]), &epwmParams.sin_frequency, 0, 150);
                    i += (int)log10((double)epwmParams.sin_frequency) + 2 + space;
                    space = 0;
                    break;
                case 'M':
                    populate_variable(&(buffer[i]), &epwmParams.modulation_depth, 0, 1);
                    i += (int)log10((double)epwmParams.modulation_depth) + 2 + space;
                    space = 0;
                    break;
                case 'O':
                    populate_variable(&(buffer[i]), &epwmParams.offset, -(1 - epwmParams.modulation_depth) / 2, (1 - epwmParams.modulation_depth) / 2);
                    i += (int)log10((double)epwmParams.offset) + 2 + space;
                    space = 0;
                    break;
                case 'A':
                    switch(buffer[i+1])
                    {
                    case '1':
                        populate_variable(&(buffer[i]), &epwmParams.angle_1, -360, 360);
                        i += (int)log10((double)epwmParams.angle_1) + 3 + space;
                        space = 0;
                        break;
                    case '2':
                        populate_variable(&(buffer[i]), &epwmParams.angle_2, -360, 360);
                        i += (int)log10((double)epwmParams.angle_2) + 3 + space;
                        space = 0;
                        break;
                    case '3':
                        populate_variable(&(buffer[i]), &epwmParams.angle_3, -360, 360);
                        i += (int)log10((double)epwmParams.angle_3) + 3 + space;
                        space = 0;
                        break;
                    }
                default:
                    msg = "\r\nInvalid character: ";
                    scia_msg(msg);
                    scia_xmit(buffer[i]);
                    msg = "\r\nFormat should be P 2000,S 60,M 1.0,O 0.0,A1 0.0,A2 120.0,A3 240.0";
                    scia_msg(msg);
                    i++;
                    break;
                }


                msg = "\r\nThe number you tried to send was: ";
                scia_msg(msg);
                char tempMsg[20];
                sprintf(tempMsg, "%d", (char)epwmParams.pwm_frequency); //changed int to string and adds terminating character
                scia_msg(tempMsg);
            }

            bufferIndex = 0; // Reset buffer index
            //reset buffer
            int k = 0;
            for (k = 0; k < MAX_BUFFER_SIZE; k++)
            {
                buffer[k] = '\0';
            }
        }
        else
        {
            // Add character to buffer if there's space
            if (bufferIndex < MAX_BUFFER_SIZE - 1)
            {
                buffer[bufferIndex++] = ReceivedChar;
            }
            else
            {
                msg = "To many characters added";
                scia_msg(msg);
            }
        }

        LoopCount++;
    }
}

//this function takes in where we left off in the array, and returns an int value
int populate_variable(const char *arr, float *var, float min, float max)
{
    char temp[10] = { '\0' };
    int i = 1; //goes to next spot in array (we were looking at the letter before)
    int j = 0;

//checks for space between letter and value, ex F 10000
    if (arr[i] == ' ')
        i++;

    while (arr[i] != '\0' && arr[i] >= '0' && arr[i] <= '9') //check to make sure array is a number
    {
        temp[j++] = arr[i];   //populates temp array to hold value
        i++;                //keep track of where we are in the array
    }

    // Converts characters to float, assigns to *var
    *var = atof(temp);
    if (*var < min || *var > max)
    {
        char errorMsg[MAX_MSG_SIZE];
        sprintf(errorMsg, "\r\nValue out of bounds: %f", *var);
        scia_msg(errorMsg);
        *var = (min + max) / 2; // Set to a default value within bounds
    }
    return 0;
}

// scia_echoback_init - Test 1,SCIA  DLB, 8-bit word, baud rate 0x0103, 
// default, 1 STOP bit, no parity
void scia_echoback_init()
{
// Note: Clocks were turned on to the SCIA peripheral
// in the InitSysCtrl() function

// 1 stop bit,  No loopback, No parity,8 char bits, async mode, idle-line protocol

    SciaRegs.SCICCR.all = 0x0007;


// enable TX, RX, internal SCICLK, Disable RX ERR, SLEEP, TXWAKE
    SciaRegs.SCICTL1.all = 0x0003;
    SciaRegs.SCICTL2.bit.TXINTENA = 0;
    SciaRegs.SCICTL2.bit.RXBKINTENA = 0;


// 9600 baud @LSPCLK = 22.5MHz (90 MHz SYSCLK)
    SciaRegs.SCIHBAUD = 0x0001;
    SciaRegs.SCILBAUD = 0x0024;

    SciaRegs.SCICTL1.all = 0x0023;  // Relinquish SCI from Reset
}

// scia_xmit - Transmit a character from the SCI
void scia_xmit(int a)
{
    while (SciaRegs.SCIFFTX.bit.TXFFST != 0)
    {
        // Wait for the transmit buffer to be empty

    }
    SciaRegs.SCITXBUF = a; //placed below to ensure that the transmit buffer is empty before attempting to send data
}

//
// scia_msg - 
//
void scia_msg(char *msg)
{
    int i = 0;
    while (msg[i] != '\0') //keeps transmitting data until end of line (enter has been pressed, denoted by '\0')
    {
        scia_xmit(msg[i]); //turns the letter to be represented by a number (ascii)
        i++; //increases to next element in the array
    }
}

// scia_fifo_init - Initalize the SCI FIFO
void scia_fifo_init()
{

    SciaRegs.SCIFFTX.all = 0xE040;
    SciaRegs.SCIFFRX.all = 0x2044;
    SciaRegs.SCIFFCT.all = 0x0;
}
// End of File
