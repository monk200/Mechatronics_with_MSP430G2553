/******************************************************************************
MSP430G2553 Project Creator

SE 423  - Dan Block
        Spring(2019)

        Written(by) : Steve(Keres)
College of Engineering Control Systems Lab
University of Illinois at Urbana-Champaign
*******************************************************************************/

/*
 * Samples the photoresistor every 1ms and echos the value to the DAC through SPI
 */

#include "msp430g2553.h"
#include "UART.h"

void print_every(int rate);

char newprint = 0;
int timecheck = 0;
int firsttime = 1;
int dacValue = 0;
int dacToSend = 0;
int adcValue = 0;

void main(void) {

    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);

    DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
    BCSCTL1 = CALBC1_16MHZ;

    // Initializing ADC10
    ADC10CTL0 = ADC10SHT_1 + ADC10ON + ADC10IE;
    ADC10CTL1 = INCH_3;                      // Use A3 as input
    ADC10AE0 = 0x09;

    // Initialize Port 1
    P1SEL &= ~0xff;                  // all GPIO
    P1SEL &= ~0xff;                  // all GPIO
    P1REN &= ~0xff;                  // all resistors disabled
    P1DIR |= 0xf0;                     // Set P1.4-.7 to output direction
    P1OUT |= 0xb0;                     // Set P1.4,.5,.7 to 1, set P1.6 to 0 (low)
    // Port 1 SPI pins
    P1SEL |= BIT5 + BIT7;              // Secondary Peripheral Module Function for P1.5 and P1.7
    P1SEL2 |= BIT5 + BIT7;             // Secondary Peripheral Module Function for P1.5 and P1.7

    // Initialize Port 2
    P2DIR |= 0x04;   //Sets P2.2 as an output
    P2SEL |= 0x04;   //initializes pin as TA1.1
    P2SEL2 &= ~0x04; //initializes pin as TA1.1
    TA1CCR0 = 10000; // 10kHz carrier frequency
    TA1CCTL1 = OUTMOD_7; //TA1CCR1 reset/set
    TA1CTL = TASSEL_2 + MC_1; //SMCLK, up mode

    // Initialize SPI
    UCB0CTL0 = UCCKPL + UCCKPH + UCMSB + UCMST + UCSYNC;  // falling edge + normally high + MSB first + (8-bit data) + master mode + (2-pin SPI) + synchronous mode
    UCB0CTL1 = UCSSEL_2 + UCSWRST;               // SMCLK + enable SW Reset
    UCB0BR0 = 80;                                 // /8 Divider
    UCB0BR1 = 0;                                 //
    UCB0CTL1 &= ~UCSWRST;                        // **Initialize USCI state machine**
    IFG2 &= ~UCB0RXIE;                        // Clear RX interrupt flag in case it was set during init
    IE2 |= UCB0RXIE;                          // Enable USCI0 B0RX interrupt

    // Timer A Config
    TACCTL0 = CCIE;             // Enable Periodic interrupt
    TACCR0 = 16000;                // period = 1ms
    TACTL = TASSEL_2 + MC_1;    // source SMCLK, up mode

    Init_UART(115200,1);    // Initialize UART for 115200 baud serial communication

    _BIS_SR(GIE);       // Enable global interrupt

    while(1) {  // Low priority Slow computation items go inside this while loop.  Very few (if anyt) items in the HWs will go inside this while loop
        // The newprint variable is set to 1 inside the function "print_every(rate)" at the given rate
        if ( (newprint == 1) && (senddone == 1) )  { // senddone is set to 1 after UART transmission is complete
            // only one UART_printf can be called every 15ms
            UART_printf("dac: %d adc: %d firsttime: %d\n\r", dacValue, adcValue, firsttime);
            newprint = 0;
        }
    }
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    timecheck++; // Keep track of time for main while loop.
    print_every(500);  // units determined by the rate Timer_A ISR is called, print every "rate" calls to this function

    ADC10CTL0 |= ENC + ADC10SC; // Sampling and conversion start
}

// ADC 10 ISR - Called when a sequence of conversions (A7-A0) have completed
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
    adcValue = ADC10MEM;

    /*dacValue++;     // simple ramp from 0 V to Vcc
    if (dacValue == 0x3ff) {        // max dacValue 0011 1111 1111 = 1023
        dacValue = 0x0;     // resets dacValue when it hits the max
    }*/

    dacValue = adcValue;        // sample & echo photoresistor to DAC
    if (dacValue > 0x3ff) {     // if larger than largest value, just set at max (shouldn't happen)
        dacValue = 0x3ff;
    }

    P1OUT |= BIT6;                           // Pull P1.6 high
    P1OUT &= ~BIT6;                            // Pull P1.6 low (ready to write new DAC value through SPI)

    dacToSend = (dacValue & 0x3c0) >> 6;       // bits 6-9 of dacValue
    UCB0TXBUF = 0x40 + dacToSend;                     // Pad top with 0100 (x + fast mode + normal operation + x), send bits 6-9 of dacVal
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
        if (firsttime) {
            dacToSend = dacValue & 0x3f;        // bits 0-5 of dacVal
            UCB0TXBUF = dacToSend << 2;                     // Pad bottom with 2 0's, send bits 0-5 of dacVal
            firsttime = 0;
        } else {        // we have entered RX interrupt once before, this time do nothing (besides reset the flag and increment to next dacValue)
            firsttime = 1;
        }
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
        }*/
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
