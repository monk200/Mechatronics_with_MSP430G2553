/******************************************************************************
MSP430G2553 Project Creator

SE 423  - Dan Block
        Spring(2019)

        Written(by) : Steve(Keres)
College of Engineering Control Systems Lab
University of Illinois at Urbana-Champaign
*******************************************************************************/

/*
 * Prints 0, 1, 2, or 3 to the serial port corresponding to the state of the 2 switches soldered to the PCB and lights corresponding LEDs
 */

#include "msp430g2553.h"
#include "UART.h"

void print_every(int rate);

char newprint = 0;
long NumOn = 0;
long NumOff = 0;
int statevar = 1;
int timecheck = 0;

// return which switch is pressed
char get_switchstate(void) {
    if ((P2IN & 0xc0) == 0) return 3;       // if P2.6 and P2.7 are 0 (pressed), return 3
    else if ((P2IN & 0x40) == 0) return 1;      // if P2.6 is 0 (pressed), return 1
    else if ((P2IN & 0x80) == 0) return 2;      // if P2.7 is 0 (pressed), return 2

    return 0;       // defaults to return 0 if neither button is pressed
}

void main(void) {

    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);

    DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
    BCSCTL1 = CALBC1_16MHZ;

    // Initialize Port 1
    P1SEL &= ~0x01;  // See page 42 and 43 of the G2553's datasheet, It shows that when both P1SEL and P1SEL2 bits are zero
    P1SEL2 &= ~0x01; // the corresponding pin is set as a I/O pin.  Datasheet: http://coecsl.ece.illinois.edu/ge423/datasheets/MSP430Ref_Guides/msp430g2553datasheet.pdf
    P1REN = 0x0;  // No resistors enabled for Port 1
    P1DIR |= 0xf1; // Set P1.0 to output to drive LED on LaunchPad board.  Make sure shunt jumper is in place at LaunchPad's Red LED
    P1OUT &= ~0x01;  // Initially set P1.0 to

    // Initialize Port 2
    P2SEL = 0x0;  // 0 for hw1, |= with 0 does nothing!!
    P2SEL2 = 0x0; // 0 for hw1
    P2REN |= 0xc0;  // enables resistors for ports 2.6 and 2.7
    P2DIR = 0x0; // sets all port 2's to be inputs
    P2OUT |= 0xc0;  // sets initial values for ports 2.6 and 2.7 to be 1 == not pressed

    // Timer A Config
    TACCTL0 = CCIE;             // Enable Periodic interrupt
    TACCR0 = 16000;                // period = 1ms
    TACTL = TASSEL_2 + MC_1; // source SMCLK, up mode


    Init_UART(115200,1);    // Initialize UART for 115200 baud serial communication
    _BIS_SR(GIE);       // Enable global interrupt


    while(1) {  // Low priority Slow computation items go inside this while loop.  Very few (if anyt) items in the HWs will go inside this while loop
        // The newprint variable is set to 1 inside the function "print_every(rate)" at the given rate
        if ( (newprint == 1) && (senddone == 1) )  { // senddone is set to 1 after UART transmission is complete

            // print which switch is pressed
            char myswitch = get_switchstate();
            UART_printf("State: %d\n\r", (int) myswitch);

            newprint = 0;
        }
    }
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    timecheck++; // Keep track of time for main while loop.
    print_every(500);  // units determined by the rate Timer_A ISR is called, print every "rate" calls to this function

    // light LED's based off buttons
    P1OUT &= ~0xf1;     // turns off all LEDs
    statevar = ((int) get_switchstate()) + 1;       // adds 1 to get_switchstate() because switch statements start at case 1
    switch (statevar) {
        case 1:     // neither button pressed
            if (timecheck >= 500) timecheck = 0;      // reset timer
            else if (timecheck < 150) P1OUT |= 0x11;      // turn LED 1 on
            else P1OUT &= ~0xf1;     // turn off
            break;
        case 2:     // button 1 pressed
            if (timecheck >= 500) timecheck = 0;      // reset timer
            else if (timecheck < 150) P1OUT |= 0x21;      // turn LED 2 on
            else P1OUT &= ~0xf1;     // turn off
            break;
        case 3:     // button 2 pressed
            if (timecheck >= 500) timecheck = 0;      // reset timer
            else if (timecheck < 150) P1OUT |= 0x41;      // turn LED 3 on
            else P1OUT &= ~0xf1;     // turn off
            break;
        case 4:     // both buttons pressed
            if (timecheck >= 500) timecheck = 0;      // reset timer
            else if (timecheck < 150) P1OUT |= 0x81;      // turn LED 4 on
            else P1OUT &= ~0xf1;     // turn off
            break;
    }
}


/*
// ADC 10 ISR - Called when a sequence of conversions (A7-A0) have completed
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {

}
*/


// USCI Transmit ISR - Called when TXBUF is empty (ready to accept another character)
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {
    if(IFG2&UCA0TXIFG) {        // USCI_A0 requested TX interrupt
        if(printf_flag) {
            if (currentindex == txcount) {
                senddone = 1;
                printf_flag = 0;
                IFG2 &= ~UCA0TXIFG;
            } else {
                UCA0TXBUF = printbuff[currentindex];
                currentindex++;
            }
        } else if(UART_flag) {
            if(!donesending) {
                UCA0TXBUF = txbuff[txindex];
                if(txbuff[txindex] == 255) {
                    donesending = 1;
                    txindex = 0;
                }
                else txindex++;
            }
        } else {  // interrupt after sendchar call so just set senddone flag since only one char is sent
            senddone = 1;
        }

        IFG2 &= ~UCA0TXIFG;
    }

    if(IFG2&UCB0TXIFG) {    // USCI_B0 requested TX interrupt (UCB0TXBUF is empty)
        IFG2 &= ~UCB0TXIFG;   // clear IFG
    }
}


// USCI Receive ISR - Called when shift register has been transferred to RXBUF
// Indicates completion of TX/RX operation
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
    if(IFG2&UCB0RXIFG) {  // USCI_B0 requested RX interrupt (UCB0RXBUF is full)
        IFG2 &= ~UCB0RXIFG;   // clear IFG
    }

    if(IFG2&UCA0RXIFG) {  // USCI_A0 requested RX interrupt (UCA0RXBUF is full)

//    Uncomment this block of code if you would like to use this COM protocol that uses 253 as STARTCHAR and 255 as STOPCHAR
/*      if(!started) {  // Haven't started a message yet
            if(UCA0RXBUF == 253) {
                started = 1;
                newmsg = 0;
            }
        }
        else {  // In process of receiving a message
            if((UCA0RXBUF != 255) && (msgindex < (MAX_NUM_FLOATS*5))) {
                rxbuff[msgindex] = UCA0RXBUF;

                msgindex++;
            } else {    // Stop char received or too much data received
                if(UCA0RXBUF == 255) {  // Message completed
                    newmsg = 1;
                    rxbuff[msgindex] = 255; // "Null"-terminate the array
                }
                started = 0;
                msgindex = 0;
            }
        }
*/
        IFG2 &= ~UCA0RXIFG;
    }
}

// This function takes care of all the timing for printing to UART
// Rate determined by how often the function is called in Timer ISR
int print_timecheck = 0;
void print_every(int rate) {
    if (rate < 15) {
        rate = 15;
    }
    if (rate > 10000) {
        rate = 10000;
    }
    print_timecheck++;
    if (print_timecheck == rate) {
        print_timecheck = 0;
        newprint = 1;
    }
}
