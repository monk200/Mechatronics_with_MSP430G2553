/******************************************************************************
MSP430G2553 Project Creator

SE 423  - Dan Block
        Spring(2019)

        Written(by) : Steve(Keres)
College of Engineering Control Systems Lab
University of Illinois at Urbana-Champaign
*******************************************************************************/

/*
 * Disables button interrupt for a short time after initial interrupt to prevent illusion of multiple button taps when only one tap occurred
 * Also toggles an LED when either button is pressed
 */

#include "msp430g2553.h"
#include "UART.h"

void print_every(int rate);

char newprint = 0;
int timecheck = 0;
int button1 = 0;
int button2 = 0;
int disable = 0;
int tenmsCounter = 0;

void main(void) {
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
    if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);
    DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
    BCSCTL1 = CALBC1_16MHZ;

    // Initialize Port 1
    P1SEL &= ~0x11;  // when both P1SEL and P1SEL2 bits are zero
    P1SEL2 &= ~0x11; // the corresponding pin is set as a I/O pin
    P1REN = 0x10;  // resistor enabled for 1.4
    P1DIR |= 0x11; // Set P1.0 and 1.4 to output to drive LED
    P1OUT &= ~0x11;  // Initially set P1.0 and 1.4 to 0

    // Initialize Port 2
    P2SEL = 0x0;    // all port 2 pins are I/O pins
    P2SEL2 = 0x0;
    P2REN |= 0xc0;  // enables resistors for ports 2.6 and 2.7
    P2DIR = 0x0; // sets all port 2's to be inputs
    P2OUT |= 0xc0;  // sets initial values for ports 2.6 and 2.7 to be 1 == not pressed
    // Port 2 interrupts
    P2IE |= 0xc0;       // enables interrupt for ports 2.6 and 2.7
    P2IES |= 0xc0;        // hi/lo edge
    P2IFG &= ~0xc0;     // clears IFG

    // Timer A Config
    TACCTL0 = CCIE;             // Enable Periodic interrupt
    TACCR0 = 16000;                // period = 1ms
    TACTL = TASSEL_2 + MC_1; // source SMCLK, up mode

    Init_UART(115200,1);    // Initialize UART for 115200 baud serial communication
    _BIS_SR(GIE);       // Enable global interrupt

    while(1) {  // Low priority Slow computation items go inside this while loop.  Very few (if anyt) items in the HWs will go inside this while loop
        // The newprint variable is set to 1 inside the function "print_every(rate)" at the given rate
        if ( (newprint == 1) && (senddone == 1) )  { // senddone is set to 1 after UART transmission is complete
            // only one UART_printf can be called every 15ms
            UART_printf("button1: %d button2: %d\n\r", button1, button2);
            newprint = 0;
        }
    }
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    timecheck++; // Keep track of time for main while loop.
    print_every(500);  // units determined by the rate Timer_A ISR is called, print every "rate" calls to this function

    if (disable) {
        tenmsCounter = 0;        // reset 10ms counter
        P2IE &= ~0xc0;      // disables interrupt for ports 2.6 and 2.7
        disable = 0;
    }
    if (tenmsCounter == 380) P2IE |= 0xc0;       // enables interrupt for ports 2.6 and 2.7
    else tenmsCounter++;      // increment 10ms counter
}

// Port 2 interrupt function
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void) {
    if ((P2IFG & 0x40) == 0x40) button1++;  // P2.6 generated interrupt
    if ((P2IFG & 0x80) == 0x80) button2++;  // P2.7 generated interrupt

    P1OUT = (P1OUT ^ 0x10);     // toggle LED whenever either button is pressed

    P2IFG &= ~0xc0;     // clears IFG
    disable = 1;        // need to disable interrupt
}

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
