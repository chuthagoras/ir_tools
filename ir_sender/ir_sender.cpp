#include <string>
#include <vector>
#include <cmath>
extern "C" {
#include <pigpio.h>
}
#include <cstdlib>
#include <cstdio>

#include "ir_sender.h"
#define MAX_COMMAND_SIZE 512
#define MAX_PULSES 12000



void IrSender::addPulse(uint32_t onPins, uint32_t offPins, uint32_t duration, gpioPulse_t *irSignal, int *pulseCount)
{
  int index = *pulseCount;

  irSignal[index].gpioOn = onPins;
  irSignal[index].gpioOff = offPins;
  irSignal[index].usDelay = duration;

  (*pulseCount)++;
}

void IrSender::carrierFrequency(double duration, gpioPulse_t *irSignal, int *pulseCount)
{
  double oneCycleTime = 1000000.0 / m_frequency; // 1000000 microseconds in a second
  int onDuration = (int)round(oneCycleTime * m_duty_cycle);
  int offDuration = (int)round(oneCycleTime * (1.0 - m_duty_cycle));

  int totalCycles = (int)round(duration / oneCycleTime);
  int totalPulses = totalCycles * 2;

  int i;
  for (i = 0; i < totalPulses; i++)
  {
    if (i % 2 == 0)
    {
      // High pulse
      addPulse(1 << m_outpin, 0, onDuration, irSignal, pulseCount);
    }
    else
    {
      // Low pulse
      addPulse(0, 1 << m_outpin, offDuration, irSignal, pulseCount);
    }
  }
}

// Generates a low signal gap for duration, in microseconds, on GPIO pin m_outpin
void IrSender::gap(double duration, gpioPulse_t *irSignal, int *pulseCount)
{
  addPulse(0, 0, duration, irSignal, pulseCount);
}


//for raws, positive value = turn on for given microseconds
//          negative value = turn off for given microseconds
int IrSender::ir_send_raw(std::vector<int> pulses)
{
  if (m_outpin > 31)
  {
    // Invalid pin number
    return 1;
  }
  // Generate Code
  gpioPulse_t irSignal[MAX_PULSES];
  int pulseCount = 0;
  for (int i = 0; i < pulses.size(); i++)
  {
    if (pulses[i] >= 0) {
      carrierFrequency(pulses[i], irSignal, &pulseCount);
    } else {
      gap(-pulses[i], irSignal, &pulseCount);
    }
  }

  //  printf("pulse count is %i\n", pulseCount);
  // End Generate Code

  // Init pigpio
  if (gpioInitialise() < 0)
  {
    // Initialization failed
    printf("GPIO Initialization failed\n");
    return 1;
  }

  // Setup the GPIO pin as an output pin
  gpioSetMode(m_outpin, PI_OUTPUT);

  // Start a new wave
  gpioWaveClear();

  gpioWaveAddGeneric(pulseCount, irSignal);
  int waveID = gpioWaveCreate();

  if (waveID >= 0)
  {
    int result = gpioWaveTxSend(waveID, PI_WAVE_MODE_ONE_SHOT);

    //  printf("Result: %i\n", result);
  }
  else
  {
    printf("Wave creation failure!\n %i", waveID);
  }

  // Wait for the wave to finish transmitting
  while (gpioWaveTxBusy())
  {
    time_sleep(0.1);
  }

  // Delete the wave if it exists
  if (waveID >= 0)
  {
    gpioWaveDelete(waveID);
  }

  gpioWrite(m_outpin, 1);

  // Cleanup
  gpioTerminate();
  return 0;
}


//time_slot is the time in microseconds of a bit, header footer can be null
int IrSender::send_bi_phase(int time_slot, SendData data)
{
  std::vector<int> codes = data.header;
  //generate codes
  for (int i=0; i<data.code.size(); i++)
  {
    if (data.code[i] == '0') // go down in the middle of time_slot
    {
      codes.push_back(time_slot/2 + time_slot%2);
      codes.push_back(-time_slot/2);
    }
    else if (data.code[i] == '1')
    {
      codes.push_back(-(time_slot/2 + time_slot%2));
      codes.push_back(time_slot/2);
    }
    else
    {
      // TODO ERROR
    }
  }
  codes.insert(codes.end(), data.footer.begin(), data.footer.end());
  int output = ir_send_raw(codes);
  return output;
}

// on_length on time in micros
// zero_distance off time for 0 in micros (positive value)
// one_distance on time for 1 in micros
int IrSender::send_pulse_distance(int on_length, int zero_distance, int one_distance, SendData data)
{
  std::vector<int> codes = data.header;
  for (int i=0; i<data.code.size(); i++)
  {
    codes.push_back(on_length);
    if (data.code[i] == '0')
    {
      codes.push_back(-zero_distance);
    }
    else if (data.code[i] == '1')
    {
      codes.push_back(-one_distance);
    }
    else
    {
      printf("send_pulse_distance: Supplied code is not 0 or 1");
      // TODO error
    }
  }
  codes.insert(codes.end(), data.footer.begin(), data.footer.end());
  int output = ir_send_raw(codes);
  return output;
}

int IrSender::send_pulse_length(int off_length, int zero_burst, int one_burst, SendData data)
{
  std::vector<int> codes = data.header;
  for (int i=0; i<data.code.size(); i++)
  {
    if (data.code[i] == '0')
    {
      codes.push_back(zero_burst);
    }
    else if (data.code[i] == '1')
    {
      codes.push_back(one_burst);
    }
    else
    {
      // TODO error
    }
  }
  codes.insert(codes.end(), data.footer.begin(), data.footer.end());
  int output = ir_send_raw(codes);
  return output;
}


int IrSender::send_rc5(std::string address, std::string data)
{
  int time_slot = 1780;
  SendData s_data;
  s_data.code = "110" + address + data;
  int output = send_bi_phase(time_slot, s_data);
  return output;
}

char IrSender::flip(char a)
{
  if (a == '0') return '1';
  else if (a == '1') return '0';
  else return 'e';
}

//address + data size are both 8 bits
int IrSender::send_nec(std::string address, std::string data)
{
  SendData s_data;
  s_data.header = {9000, -4500};
  std::string flipped_address = address;
  std::string flipped_data = data;
  for (int i=0; i<flipped_address.size(); i++)
  {
    flipped_address[i] = flip(flipped_address[i]);
    flipped_data[i] = flip(flipped_data[i]);
  }
  s_data.code = address + flipped_address + data + flipped_data;
  s_data.footer = {563};
  int out = send_pulse_distance(563, 562, 1688, s_data);
  return out; //TODO some better error
}

