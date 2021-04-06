/******************************************************************************
MSP430G2553 Project Creator

SE 423  - Dan Block
        Spring(2019)

        Written(by) : Steve(Keres)
College of Engineering Control Systems Lab
University of Illinois at Urbana-Champaign
*******************************************************************************/

/*
 * Every 20 ms the two servo motors change their position to be either a 5% or 12% duty cycle
 */

#include "msp430g2553.h"
#include "UART.h"

void print_every(int rate);

char newprint = 0;
int timecheck = 0;
int toggle = 0;

void main(void) {

    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);

    DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
    BCSCTL1 = CALBC1_16MHZ;

    // Initialize Port 1
    P1SEL &= ~0xff;                  // all GPIO
    P1SEL &= ~0xff;                  // all GPIO
    P1REN &= ~0xff;                  // all resistors disabled
    P1DIR |= 0xf0;                     // Set P1.4-.7 to output direction
    P1OUT = 0x0;                     // Set P1.4-.7 to 0 (low)

    // Initialize Port 2
    P2DIR |= 0x12;   //Sets P2.1 (RC1) and P2.4 (RC2) as an output
    P2SEL |= 0x12;   //initializes pins as TA1.1
    P2SEL2 &= ~0x12; //initializes pins as TA1.1

    // Timer A0 Config
    TA0CCTL0 = CCIE;             // Enable Periodic interrupt
    TA0CCR0 = 16000;                // period = 1ms
    TA0CTL = TASSEL_2 + MC_1;       // source SMCLK, up mode

    // Timer A1 Config
    TA1CCTL1 = OUTMOD_7;       //TA1CCR1 PWM reset/set
    TA1CCTL2 = OUTMOD_7;       //TA1CCR1 PWM reset/set
    TA1CCTL0 = 0;
    TA1CCR0 = 40000;        // 50Hz carrier frequency
    TA1CTL = TASSEL_2 + ID_3 + MC_1;        //SMCLK + input divide by 8 + up mode

    Init_UART(115200,1);    // Initialize UART for 115200 baud serial communication

    _BIS_SR(GIE);       // Enable global interrupt

    while(1) {  // Low priority Slow computation items go inside this while loop.  Very few (if anyt) items in the HWs will go inside this while loop
        // The newprint variable is set to 1 inside the function "print_every(rate)" at the given rate
        if ( (newprint == 1) && (senddone == 1) )  { // senddone is set to 1 after UART transmission is complete
            // only one UART_printf can be called every 15ms
            UART_printf("toggle: %d\n\r", toggle);
            newprint = 0;
        }
    }
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    timecheck++; // Keep track of time for main while loop.
    print_every(500);  // units determined by the rate Timer_A ISR is called, print every "rate" calls to this function

    // toggles duty cycle every 20ms
    if (timecheck%2000 == 0) {
        toggle = !toggle;
    }

    if (toggle == 0) {
        TA1CCR1 = 2000;       //duty cycle of PWM is 5%
        TA1CCR2 = 2000;
    }
    if (toggle == 1) {
        TA1CCR1 = 4800;       //duty cycle of PWM is 12%
        TA1CCR2 = 4800;
    }
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
