/******************************************************************************
MSP430G2553 Project Creator

SE 423  - Dan Block
        Spring(2019)

        Written(by) : Steve(Keres)
College of Engineering Control Systems Lab
University of Illinois at Urbana-Champaign
*******************************************************************************/

/*
 * Uses the Capture functionality of TimerA1 to display the period and time that TimerA1 is high to the terminal
 */

#include "msp430g2553.h"
#include "UART.h"

void print_every(int rate);

char newprint = 0;
int timecheck = 0;
unsigned int TA1Period = 0;
unsigned int last_rising = 0;
unsigned int high_time = 0;

void main(void) {
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
    if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);
    DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
    BCSCTL1 = CALBC1_16MHZ;

    // Initialize Port 1
    P1SEL &= ~0x01;  //  Make sure P1.0 GPIO
    P1SEL |= 0x40;  // Set P1.6 as TA0.1
    P1SEL2 &= ~0x41; // P1.0 GPIO P1.6 TA0.1
    P1REN = 0x0;  // No resistors enabled for Port 1
    P1DIR |= 0x41; // Set P1.0 output P1.6 TA0.1
    P1OUT &= ~0x01;  // Initially set P1.0 to 0

    // TODO: Initialize Port 2.1 as TA1.CCI1A capture input pin
    P2SEL |= 0x02;      // P2.1 = 1
    P2SEL2 &= ~0x02;        //P2.1 = 0 (GPIO)
    P2DIR &= ~0x02;     //P2.1 = 0 (input)

    // Timer0 A Config  So this sets Timer0 to interrupt every 1ms and generate a PWM signal on P1.6
    TA0CCTL0 = CCIE;       // Enable Periodic interrupt
    TA0CCR0 = 16000;       // period = 1ms
    TA0CCR1 = 8000;        // Start with 50% duty cycle
    TA0CCTL1 = OUTMOD_7;   // Set/Reset
    TA0CTL = ID_3 + TASSEL_2 + MC_1; // divide by 1, source SMCLK, up mode

    // TODO : Timer A1 Config  Set Timer A1 in capture mode and use capture pin TA1.CCI1A at P2.1
    TA1CCTL1 = CM_3 + SCS + CAP + CCIE;      // Capture on rising & falling edge + (CCI1A) + Synchronous mode + Capture mode + Interrupt enabled
                                            // TA1CCR1 Capture mode; CCI1A; Both; Rising and Falling Edge; interrupt enable CCIS_0
    TA1CTL = TASSEL_2 + ID_0 + MC_2 + TACLR;        // SMCLK + /1 input divider + continuous mode + timer_A clear
                                                    // SMCLK, Continous Mode; Clear timer

    Init_UART(115200,1);    // Initialize UART for 115200 baud serial communication
    _BIS_SR(GIE);       // Enable global interrupt

    while(1) {
        if ( (newprint == 1) && (senddone == 1) )  {
            UART_printf("TA1Period: %uus, High Time: %uus\n\r", (int)(TA1Period*1000L)/16000, (int)(high_time*1000L)/16000);
            newprint = 0;
        }
    }
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    timecheck++; // Keep track of time for main while loop.
    print_every(250);  // units determined by the rate Timer_A ISR is called, print every "rate" calls to this function
    if (timecheck == 500) {
        P1OUT ^= 0x1;
        if (TACCR1<(TACCR0-1000)){
            TACCR1 = TACCR1+1000;
        } else {
            TACCR1 = 4000;
        }
        timecheck = 0;
    }
}

// TA1_A1 Interrupt vector
#pragma vector = TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR (void) {
  unsigned int curr = TA1CCR1;
  switch(TA1IV) {
      case  TA1IV_NONE:
          // Should not get here
          break;
      case  TA1IV_TACCR1:  // TACCR1 CCIFG  Capture interrupt
          //TODO  Read TACCR1 to know when the Capture interrupt occurred and then additional code to perform given tasks.
          if ((CCI & TA1CCTL1) == 0x08) {        // rising edge
              TA1Period = curr - last_rising;
              last_rising = curr;
          } else {      // falling edge
              high_time = curr - last_rising;
          }
          break;
      case TA1IV_TACCR2: break; // TACCR2 CCIFG Not used in HW6
      case TA1IV_6: break;      // Reserved CCIFG Not used
      case TA1IV_8: break;      // Reserved CCIFG Not used
      case TA1IV_TAIFG:         // TAIFG  Overflow interrupt. Used in Challenge part

          break;
      default:  break;
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
