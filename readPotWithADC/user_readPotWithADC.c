/******************************************************************************
MSP430G2553 Project Creator

SE 423  - Dan Block
        Spring(2019)

        Written(by) : Steve(Keres)
College of Engineering Control Systems Lab
University of Illinois at Urbana-Champaign
*******************************************************************************/

/*
 * Uses ADC0 to read from a potentiometer and turns on LEDs corresponding to how much voltage is passing through
 * < .25 * Vcc = all LEDs off
 * > .25 * Vcc = 2 LEDs on
 * > .5 * Vcc = 4 LEDs on
 */

#include "msp430g2553.h"
#include "UART.h"

void print_every(int rate);

char newprint = 0;
int timecheck = 0;
char done = 0;
char recchar = 0;

void main(void) {
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);

    DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
    BCSCTL1 = CALBC1_16MHZ;

    // Initialize Port 1
    P1SEL &= ~0x01;  // See page 42 and 43 of the G2553's datasheet, It shows that when both P1SEL and P1SEL2 bits are zero
    P1SEL2 &= ~0x01; // the corresponding pin is set as a I/O pin.  Datasheet: http://coecsl.ece.illinois.edu/ge423/datasheets/MSP430Ref_Guides/msp430g2553datasheet.pdf
    P1REN = 0x0;  // No resistors enabled for Port 1
    P1DIR |= 0xfe; // P1.0 is analog input, others are outputs: 1111 1110 = 0xfe
    //P1DIR |= 0xfb; // P1.3 is analog input, others are outputs: 1111 1011 = 0xfb
    P1OUT = 0x0;  // Initially set everything off

    // Initialize Port 2
    P2SEL |= 0x04;        // P2.2 TA1.1 option
    P2SEL2 &= ~0x04;      // P2.2 TA1.1 option
    P1REN = 0x0;        // No resistors enabled for Port 2
    P2DIR |= 0x04;        // P2.2 output
    P2OUT &= ~0x04;       // Does not affect P2.2 because not in GPIO function

    TA1CCR1 = 0xa;      // set TA1CCR1 to 10kHz

    // Initialize ADC10
    ADC10CTL0 = ADC10SHT_1 + ADC10IE;
    //ADC10DIV0 = 0;      // 000 = div(1), 011 = div(4), 111 = div(8)
    //ADC10DIV1 = 0;
    //ADC10DIV2 = 0;
    ADC10AE0 |= 0x01;       // enable A0 as ADC channel
    //ADC10AE0 |= 0x08;       // enable A3 as ADC channel

    // Timer A Config
    TACCTL0 = CCIE;             // Enable Periodic interrupt
    TACCR0 = 16000;                // period = 1ms
    TACTL = TASSEL_2 + MC_1; // source SMCLK, up mode

    Init_UART(115200,1);    // Initialize UART for 115200 baud serial communication

    _BIS_SR(GIE);       // Enable global interrupt

    while(1) {  // Low priority Slow computation items go inside this while loop.  Very few (if anyt) items in the HWs will go inside this while loop
// for use if you want to use a method of receiving a string of chars over the UART see USCI0RX_ISR below
//      if(newmsg) {
//          newmsg = 0;
//      }
        // The newprint variable is set to 1 inside the function "print_every(rate)" at the given rate
        if ( (newprint == 1) && (senddone == 1) )  { // senddone is set to 1 after UART transmission is complete
            // only one UART_printf can be called every 15ms
            UART_printf("Analog input: %d\n\r",ADC10MEM);
            newprint = 0;
        }
    }
}


// Timer A0 interrupt service routine, every 1ms
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void){
    timecheck++; // Keep track of time for main while loop.
    print_every(250);  // print every 250ms

    ADC10CTL0 |= ENC + ADC10SC; // Sampling and conversion start
    done = 0;                   // Reset the flag after starting ADC
    while (done == 0) {
        //do nothing
    }
}



// ADC 10 ISR - Called when a sequence of conversions (A7-A0) have completed
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
    if (ADC10MEM < 0xff) {
        P1OUT &= ~0xff;        // all LEDs off
    } else if (ADC10MEM < 0x1ff) {
        P1OUT |= 0x11;       // two LEDs on
        P1OUT |= 0x21;
    } else {
        P1OUT |= 0x11;       // four LEDs on
        P1OUT |= 0x21;
        P1OUT |= 0x41;
        P1OUT |= 0x81;
    }

    done = 1;
}

// USCI Transmit ISR - Called when TXBUF is empty (ready to accept another character)
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {
    if(IFG2&UCA0TXIFG) {        // USCI_A0 requested TX interrupt
        recchar = UCA0RXBUF;
        sendchar(recchar);

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

