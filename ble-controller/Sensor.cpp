#include "Sensor.h"
#include "MPU6050.h"
#include "Adafruit_VL53L0X.h"
#include <math.h>
#include <vector>
#include <map>
// #include <BLEMidi.h>

struct InitialValue {
  int16_t floor;
  int16_t ceil;
  int16_t threshold;
  int16_t debounce;

  int16_t getValue(std::string &valueType) {
    if (valueType == "floor") {
      return floor;
    }
    if (valueType == "ceil") {
      return ceil;
    }
    if (valueType == "threshold") {
      return threshold;
    }
    if (valueType == "debounce") {
      return debounce;
    }
    return -1;
  }
};

static std::map<std::string, int> IMU_CONSTANTS = {
  { "floor", 60 },
  { "ceil", 15700 },
  { "filter_threshold", 80 },
  { "debounce", 0 }
};

static std::map<std::string, InitialValue> values = {
  { "potentiometer", { 20, 1023, 30, 0 } },
  { "force", { 20, 1023, 20, 15 } },
  { "sonar", { 6, 30, 40, 100 } },
  { "ax", { IMU_CONSTANTS["floor"], IMU_CONSTANTS["ceil"], IMU_CONSTANTS["filter_threshold"], IMU_CONSTANTS["debounce"] } },
  { "ay", { IMU_CONSTANTS["floor"], IMU_CONSTANTS["ceil"], IMU_CONSTANTS["filter_threshold"], IMU_CONSTANTS["debounce"] } },
  { "az", { IMU_CONSTANTS["floor"], IMU_CONSTANTS["ceil"], IMU_CONSTANTS["filter_threshold"], IMU_CONSTANTS["debounce"] } },
  { "gx", { IMU_CONSTANTS["floor"], IMU_CONSTANTS["ceil"], IMU_CONSTANTS["filter_threshold"], IMU_CONSTANTS["debounce"] } },
  { "gy", { IMU_CONSTANTS["floor"], IMU_CONSTANTS["ceil"], IMU_CONSTANTS["filter_threshold"], IMU_CONSTANTS["debounce"] } },
  { "gz", { IMU_CONSTANTS["floor"], IMU_CONSTANTS["ceil"], IMU_CONSTANTS["filter_threshold"], IMU_CONSTANTS["debounce"] } },
};

int16_t Sensor::getConstValue(std::string &sensorType, std::string valueType) {
  return values[sensorType].getValue(valueType);
}

Sensor::Sensor(const std::string &sensorType, const uint8_t &controllerNumber, const uint8_t &pin, const uint8_t &intPin) {
  _pin = pin;
  _controllerNumber = controllerNumber;
  _channel = 0;
  _statusCode = 176;
  _intPin = intPin;
  _midiMessage = sensorType != "force" ? "controlChange" : "gate";
  _sensorType = sensorType;
  previousValue = 0;
  previousRawValue = 0;
  currentValue = 0;
  dataBuffer = 0;
  measuresCounter = 0;
  filteredExponentialValue = 0;
  _currentDebounceValue = 0;
  _previousDebounceValue = 0;
  isActive = false;
  toggleStatus = false;
  previousToggleStatus = toggleStatus;
  isAlreadyPressed = false;
  _floor = Sensor::getConstValue(_sensorType, "floor");
  _ceil = Sensor::getConstValue(_sensorType, "ceil");
  _threshold = Sensor::getConstValue(_sensorType, "threshold");
  _debounceThreshold = Sensor::getConstValue(_sensorType, "debounce");
  // _floor = Sensor::getFloor(_sensorType);
  // _ceil = Sensor::getCeil(_sensorType);
  // _threshold = Sensor::getFilterThreshold(_sensorType);
  // _debounceThreshold = Sensor::getDebounceThreshold(_sensorType);
  msb = 0;
  lsb = 0;
}

bool Sensor::isSwitchActive() {
  return !!this->_intPin ? !!digitalRead(this->_intPin) : true;
};

bool Sensor::isAboveThreshold() {
  return this->measuresCounter % _threshold == 0;
};

void Sensor::setCurrentValue(uint8_t value) {
  this->currentValue = value;
}

void Sensor::setThreshold(uint8_t value) {
  this->_threshold = value;
}

void Sensor::setMidiMessage(std::string value) {
  this->_midiMessage = value;
}

void Sensor::setPreviousValue(uint8_t value) {
  this->previousValue = value;
}

void Sensor::setPreviousRawValue(int16_t value) {
  this->previousRawValue = value;
}

void Sensor::setMidiChannel(uint8_t channel) {
  this->_channel = channel;
}

void Sensor::setCurrentDebounceValue(unsigned long timeValue) {
  this->_currentDebounceValue = timeValue;
}

void Sensor::setMeasuresCounter(uint8_t value) {
  this->measuresCounter = !value ? value : this->measuresCounter + value;
}

void Sensor::setDataBuffer(int16_t value) {
  this->dataBuffer = !value ? value : this->dataBuffer + value;
}

std::string Sensor::getSensorType() {
  return this->_sensorType;
}

int16_t Sensor::getRawValue(MPU6050 *accelgyro, Adafruit_VL53L0X *lox) {
  if (_sensorType == "potentiometer" || _sensorType == "force" || _sensorType == "sonar") {
    return analogRead(_pin);
  }

  if (_sensorType == "infrared") {
    VL53L0X_RangingMeasurementData_t measure;

    lox->rangingTest(&measure, false);

    return measure.RangeStatus != 4 && measure.RangeMilliMeter >= _floor / 2 ? measure.RangeMilliMeter : this->previousRawValue;
  }

  if (_sensorType == "ax") {
    const int16_t rawValue = accelgyro->getAccelerationX();
    return constrain(rawValue, 0, _ceil);
  }

  if (_sensorType == "ay") {
    const int16_t rawValue = accelgyro->getAccelerationY();
    return constrain(rawValue, 0, _ceil);
  }

  if (_sensorType == "az") {
    const int16_t rawValue = accelgyro->getAccelerationZ();
    return constrain(rawValue, 0, _ceil);
  }

  if (_sensorType == "gx") {
    const int16_t rawValue = accelgyro->getRotationX();
    return constrain(rawValue, 0, _ceil);
  }

  if (_sensorType == "gy") {
    const int16_t rawValue = accelgyro->getRotationY();
    return constrain(rawValue, 0, _ceil);
  }

  if (_sensorType == "gz") {
    const int16_t rawValue = accelgyro->getRotationZ();
    return constrain(rawValue, 0, _ceil);
  }

  return 0;
}


int16_t Sensor::runNonBlockingAverageFilter() {
  return this->dataBuffer / _threshold;
}

int16_t Sensor::runBlockingAverageFilter(int measureSize, MPU6050 *accelgyro, Adafruit_VL53L0X *lox, int gap) {
  int buffer = 0;
  for (int i = 0; i < measureSize; i++) {
    int16_t value = this->getRawValue(accelgyro, lox);
    if (value < 0) {
      value = 0;
    }
    buffer += value;
    delayMicroseconds(gap);
  }
  const int16_t result = buffer / measureSize;
  return result;
}

int16_t Sensor::runExponentialFilter(MPU6050 *accelgyro, Adafruit_VL53L0X *lox) {
  static const float alpha = 0.5;
  const int16_t rawValue = this->getRawValue(accelgyro, lox);
  const float filteredValue = rawValue * alpha + (1 - alpha) * rawValue;
  return int(filteredValue);
}

std::vector< uint8_t > Sensor::getValuesBetweenRanges(uint8_t gap) {
  uint8_t samples = 1;
  if (currentValue > previousValue) {
    samples = currentValue - previousValue;
  }
  if (currentValue < previousValue) {
    samples = previousValue - currentValue;
  }
  std::vector< uint8_t > steps(samples / gap);
  uint8_t startValue = previousValue;
  std::generate(steps.begin(), steps.end(), [&startValue, this, &gap]() {
    if (this->currentValue > this->previousValue) {
      return startValue += gap;
    }
    if (this->currentValue < this->previousValue) {
      return startValue -= gap;
    }
    return startValue;
  });
  return steps;
}

int Sensor::getMappedMidiValue(int16_t actualValue, int floor, int ceil) {
  if (floor && ceil) {
    return constrain(map(actualValue, floor, ceil, 0, 127), 0, 127);
  }
  if (this->_midiMessage == "pitchBend") {
    const int pitchBendValue = constrain(map(actualValue, _floor, _ceil, 8191, 16383), 8191, 16383);
    int shiftedValue = pitchBendValue << 1;
    this->msb = highByte(shiftedValue);
    this->lsb = lowByte(shiftedValue) >> 1;
    return pitchBendValue;
  }
  return constrain(map(actualValue, _floor, _ceil, 0, 127), 0, 127);
}

void Sensor::debounce(MPU6050 *accelgyro, Adafruit_VL53L0X *lox) {
  if (_sensorType == "sonar") {
    if (this->_currentDebounceValue - this->_previousDebounceValue >= _debounceThreshold) {
      const int16_t rawValue = this->getRawValue(accelgyro, lox);
      const uint8_t sensorMappedValue = this->getMappedMidiValue(rawValue);
      if (this->currentValue != sensorMappedValue) {
        this->currentValue = this->previousValue;
      }
      this->_previousDebounceValue = this->_currentDebounceValue;
    }
  }
  if (_sensorType == "force") {
    this->previousToggleStatus = this->toggleStatus;
    if (this->_currentDebounceValue - this->_previousDebounceValue >= _debounceThreshold) {
      const int16_t rawValue = this->getRawValue(accelgyro, lox);
      const uint8_t sensorMappedValue = this->getMappedMidiValue(rawValue);
      this->toggleStatus = !!sensorMappedValue ? true : false;
      this->_previousDebounceValue = this->_currentDebounceValue;
    }
  }
}

void Sensor::sendSerialMidiMessage(HardwareSerial *Serial2) {
  if (this->_midiMessage == "controlChange") {
    if (this->currentValue != this->previousValue) {
      Sensor::writeSerialMidiMessage(this->_statusCode, this->_controllerNumber, this->currentValue, Serial2);
    }
  }
  if (this->_midiMessage == "gate") {
    if (this->toggleStatus != this->previousToggleStatus) {
      if (this->toggleStatus) {
        Sensor::writeSerialMidiMessage(144, 60, 127, Serial2);
      } else {
        Sensor::writeSerialMidiMessage(128, 60, 127, Serial2);
      }
    }
  }
}

// void Sensor::sendBleMidiMessage(BLEMidiServerClass *serverInstance) {
//   if (this->_midiMessage == "controlChange") {
//     if (this->currentValue != this->previousValue) {
//       serverInstance->controlChange(_channel, _controllerNumber, char(this->currentValue));
//     }
//   }
// if (this->_midiMessage == "gate") {
//   if (this->toggleStatus != this->previousToggleStatus) {
//     if (this->toggleStatus) {
//       serverInstance->noteOn(_channel, char(60), char(127));
//     } else {
//       serverInstance->noteOff(_channel, char(60), char(127));
//     }
//   }
// }
//   if (this->_midiMessage == "pitchBend") {
//     if (this->currentValue != this->previousValue) {
//       serverInstance->pitchBend(_channel, this->lsb, this->msb);
//     }
//   }
// }

/**
  * TODO: check ESP32 compatibility with struct (wasn't working before).
  **/
// int16_t Sensor::getFilterThreshold(std::string &type) {
//   static std::map<std::string, int> filterThresholdValues = {
//     { "potentiometer", 30 },
//     { "force", 1 },
//     { "sonar", 1 },
//     { "ax", IMU_CONSTANTS["filter_threshold"] },
//     { "ay", IMU_CONSTANTS["filter_threshold"] },
//     { "az", IMU_CONSTANTS["filter_threshold"] },
//     { "gx", IMU_CONSTANTS["filter_threshold"] },
//     { "gy", IMU_CONSTANTS["filter_threshold"] },
//     { "gz", IMU_CONSTANTS["filter_threshold"] },
//     { "infrared", 2 },
//   };
//   return filterThresholdValues[type];
// }


// int16_t Sensor::getFloor(std::string &type) {
//   static std::map<std::string, int> floorValues = {
//     { "potentiometer", 20 },
//     { "force", 300 },
//     { "sonar", 50 },
//     { "ax", IMU_CONSTANTS["floor"] },
//     { "ay", IMU_CONSTANTS["floor"] },
//     { "az", IMU_CONSTANTS["floor"] },
//     { "gx", IMU_CONSTANTS["floor"] },
//     { "gy", IMU_CONSTANTS["floor"] },
//     { "gz", IMU_CONSTANTS["floor"] },
//     { "infrared", 70 },
//   };
//   return floorValues[type];
// }

// int16_t Sensor::getCeil(std::string &type) {
//   static std::map<std::string, int> ceilValues = {
//     { "potentiometer", 1000 },
//     { "force", 1000 },
//     { "sonar", 65 },
//     { "ax", IMU_CONSTANTS["ceil"] },
//     { "ay", IMU_CONSTANTS["ceil"] },
//     { "az", IMU_CONSTANTS["ceil"] },
//     { "gx", IMU_CONSTANTS["ceil"] },
//     { "gy", IMU_CONSTANTS["ceil"] },
//     { "gz", IMU_CONSTANTS["ceil"] },
//     { "infrared", 400 },
//   };
//   return ceilValues[type];
// }

// uint16_t Sensor::getDebounceThreshold(std::string &type) {
//   static std::map<std::string, int> debounceThresholdValues = {
//     { "force", 15 },
//     { "sonar", 100 },
//   };
//   return debounceThresholdValues[type];
// }