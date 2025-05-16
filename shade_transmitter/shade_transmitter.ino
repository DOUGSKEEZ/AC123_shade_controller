// Modified shade_transmitter.ino with comprehensive AC123-06D fixes

#include <Arduino.h>

#define TX_PIN 10  // TX on digital pin 10

// Common timing parameters
unsigned int SHORT_PULSE;
unsigned int LONG_PULSE;  
unsigned int GAP_PULSE;   
unsigned int SYNC_GAP;    
unsigned int SYNC_PULSE;  
unsigned int PACKET_GAP;  
unsigned int REPEAT_GAP;  
unsigned int LONG_GAP;    
unsigned int FINAL_GAP;   

// Transmit command types
#define CMD_TYPE_UP    0
#define CMD_TYPE_DOWN  1
#define CMD_TYPE_STOP  2

// Set timing values based on remote type
void setupTimings(byte remoteType) {
  if (remoteType == 0) { // 6-channel remote (AC123-06D)
    // Calibrated values to match original remote signal
    SHORT_PULSE = 272;   // Changed to 272 to match original remote (312 vs 364)
    LONG_PULSE = 592;    // Matching original remote
    GAP_PULSE = 192;     // Changed to 192 to match original remote (240 vs 284)
    SYNC_GAP = 4864;     // Changed to 4864 to match original remote (5024 vs 5212)
    SYNC_PULSE = 4864;   // Changed to match SYNC_GAP
    PACKET_GAP = 6029;   // Adjusted to match original remote's reset value (6028.8)
    REPEAT_GAP = 192;    // Matching GAP_PULSE
    FINAL_GAP = 10480;   // Keeping as is
  } else { // 16-channel remote (AC123-16D)
    // Values specifically adjusted based on the spectrograms
    SHORT_PULSE = 238;   // Reduced to approximate remote's 280
    LONG_PULSE = 592;    // Exact match with remote
    GAP_PULSE = 238;     // Matched to SHORT_PULSE
    SYNC_GAP = 4976;     // Exact match with remote
    SYNC_PULSE = 4976;   // Exact match with remote
    PACKET_GAP = 17630;
    REPEAT_GAP = 238;    // Matched to SHORT_PULSE
    LONG_GAP = 9924;     // Updated from spectrogram for transition (was 8736)
    FINAL_GAP = 30000;   // Very long gap for final reset
  }
}

// Send a bit (PWM) - Enhanced version with extra stability for AC123-06D
void sendBit(bool bit, bool is06D = false) {
  if (is06D) {
    // Ensures more stable timing for 06D remotes
    if (bit) {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(SHORT_PULSE);
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(LONG_PULSE);
    } else {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(LONG_PULSE);
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(SHORT_PULSE);
    }
  } else {
    // Original implementation for 16D remotes
    if (bit) {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(SHORT_PULSE);
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(LONG_PULSE);
    } else {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(LONG_PULSE);
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(SHORT_PULSE);
    }
  }
}

// Send a byte - Enhanced for AC123-06D
void sendByte(byte b, bool is06D = false) {
  for (int i = 7; i >= 0; i--) {
    sendBit(bitRead(b, i), is06D);
  }
}

// Send half of a byte (4 bits)
void sendHalfByte(byte b, bool is06D = false) {
  for (int i = 7; i >= 4; i--) {
    sendBit(bitRead(b, i), is06D);
  }
}

// Specialized sync pattern for AC123-06D
void sendSync06D() {
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(SYNC_GAP);
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(SYNC_PULSE);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(GAP_PULSE);  // Using GAP_PULSE for more accurate timing
}

// Original sync pattern for other remotes
void sendSync() {
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(SYNC_GAP);
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(SYNC_PULSE);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(SHORT_PULSE);
}

// Send long pause (16ch remotes)
void sendLongGap() {
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(LONG_GAP);
}

// Send final long pause at end of transmission
void sendFinalGap() {
  // Set pin LOW for a very long time
  digitalWrite(TX_PIN, LOW);
  
  // Use delay() for much longer pauses (milliseconds)
  delay(30); // 30 milliseconds to ensure a clear gap is recorded
}

// Send trailing bit of 0 (observed to be sent as "00" in captures)
void sendTrailingZero(bool is06D = false) {
  // Send single bit 0 (which seems to appear as "00" in the receiver)
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(LONG_PULSE);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(SHORT_PULSE);
}

// Send trailing half-bit at the end of final packet
void sendFinalTrailingZero(bool is06D = false) {
  // This correctly sends the final trailing bit that appears as "0" 
  // instead of "8" in the capture
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(LONG_PULSE);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(SHORT_PULSE);
}

// Specialized function for transmitting AC123-06D commands
void transmit06DCommand(byte* header, byte* id, byte* cmd, byte commonByte, byte cmdType) {
  // Start by sending prefix byte - always 0xFF for 06D
  sendByte(0xFF, true);
  
  // Send the initial sync pulse pattern with precisely calibrated timing
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(SYNC_PULSE);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(GAP_PULSE);  // This initial gap appears to be critical
  
  // Prepare command bytes
  byte cmdBytes[8];
  memcpy(cmdBytes, header, 4);
  memcpy(cmdBytes + 4, id, 2);
  memcpy(cmdBytes + 6, cmd, 2);
  
  // Handle STOP command differently - 6 repetitions, no common section
  if (cmdType == CMD_TYPE_STOP) {
    // Adjusted to send 6 packets for STOP
    for (int i = 0; i < 6; i++) {
      // Send command bytes with 06D optimized timing
      for (int j = 0; j < 8; j++) {
        sendByte(cmdBytes[j], true);
      }
      
      // For all packets except last, add trailing 0 and sync pattern
      if (i < 5) {
        sendTrailingZero(true);
        sendSync06D();  // Use the 06D-specific sync
        delayMicroseconds(REPEAT_GAP);
      } else {
        // For the last packet, send final trailing zero
        sendFinalTrailingZero(true);
      }
    }
  } else {
    // UP and DOWN commands for 06D
    // Send command 6 times with trailing 0
    for (int i = 0; i < 6; i++) {
      for (int j = 0; j < 8; j++) {
        sendByte(cmdBytes[j], true);
      }
      sendTrailingZero(true);
      sendSync06D();  // Use the 06D-specific sync
      delayMicroseconds(REPEAT_GAP);
    }
    
    // Prepare common section with correct structure for 06D
    byte commonBytes[8];
    memcpy(commonBytes, header, 4);
    memcpy(commonBytes + 4, id, 2);
    commonBytes[6] = 0xDB;        // Fixed first byte
    commonBytes[7] = commonByte;  // Variable common byte from database
    
    // Send common message 5 times with trailing 0
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 8; j++) {
        sendByte(commonBytes[j], true);
      }
      sendTrailingZero(true);
      sendSync06D();  // Use the 06D-specific sync
      delayMicroseconds(REPEAT_GAP);
    }
    
    // Send final common message with proper trailing zero
    for (int j = 0; j < 8; j++) {
      sendByte(commonBytes[j], true);
    }
    // Send final trailing zero for the last packet
    sendFinalTrailingZero(true);
  }
  
  // Add final gap at the very end
  sendFinalGap();
}

// Transmit RF command - Main function
void transmitCommand(byte prefix, byte* header, byte* id, byte* cmd, byte remoteType, byte commonByte, bool isCC, byte cmdType) {
  Serial.print("Transmitting command for ID: ");
  Serial.print(id[0], HEX);
  Serial.print(id[1], HEX);
  Serial.print(", Type: ");
  
  if (cmdType == CMD_TYPE_UP) {
    Serial.println("UP");
  } else if (cmdType == CMD_TYPE_DOWN) {
    Serial.println("DOWN");
  } else if (cmdType == CMD_TYPE_STOP) {
    Serial.println("STOP");
  } else {
    Serial.println("UNKNOWN");
  }
  
  // Setup timings based on remote type
  setupTimings(remoteType);
  
  // Disable interrupts for more precise timing
  noInterrupts();
  
  // For AC123-06D remotes, use the specialized function
  if (remoteType == 0) {
    transmit06DCommand(header, id, cmd, commonByte, cmdType);
    interrupts(); // Re-enable interrupts
    Serial.println("Command sent (AC123-06D)");
    return;
  }
  
  // The rest of the function handles AC123-16D remotes
  // Force FE prefix for 16D
  byte actualPrefix = 0xFE;
  
  // Start sequence with correct prefix byte
  sendByte(actualPrefix);
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(SYNC_PULSE);
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(SHORT_PULSE);
  
  // Prepare command bytes
  byte cmdBytes[8];
  memcpy(cmdBytes, header, 4);
  memcpy(cmdBytes + 4, id, 2);
  memcpy(cmdBytes + 6, cmd, 2);
  
  // Handle STOP command differently - 6 repetitions, no common section
  if (cmdType == CMD_TYPE_STOP) {
    // Adjusted to send 6 packets for STOP based on your captured signal
    for (int i = 0; i < 6; i++) {
      // Send command bytes
      for (int j = 0; j < 8; j++) {
        sendByte(cmdBytes[j]);
      }
      
      // For all packets except last, add trailing 0 and sync pattern
      if (i < 5) {
        sendTrailingZero();
        sendSync();
        delayMicroseconds(REPEAT_GAP);
      } else {
        // For the last packet, send final trailing zero
        sendFinalTrailingZero();
      }
    }
    
    // Re-enable interrupts first to allow proper delay function
    interrupts();
    // Send final gap at the very end of the command
    sendFinalGap();
    
  } else {
    // 16-channel pattern (AC123-16D) for UP/DOWN commands
    // Send command 5 times with trailing 0
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 8; j++) {
        sendByte(cmdBytes[j]);
      }
      sendTrailingZero();
      sendSync();
      delayMicroseconds(REPEAT_GAP);
    }
    
    // Send command for the 6th packet
    for (int j = 0; j < 8; j++) {
      sendByte(cmdBytes[j]);
    }
    
    // Modified transition section to match the remote's pattern
    // First the long gap (9924μs)
    digitalWrite(TX_PIN, LOW);
    delayMicroseconds(LONG_GAP);
    
    // Then send 7F byte (8 bits: 01111111)
    sendByte(0x7F);
    
    // Then send 80 byte (10000000)
    sendByte(0x80);
    
    // Send sync pattern (without additional gaps)
    sendSync();
    
    // Prepare common section
    byte commonBytes[8];
    memcpy(commonBytes, header, 4);
    memcpy(commonBytes + 4, id, 2);
    commonBytes[6] = 0xDB;        // Fixed first byte
    commonBytes[7] = commonByte;  // Variable common byte from database
    
    // Send common message 5 times with trailing 0
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 8; j++) {
        sendByte(commonBytes[j]);
      }
      sendTrailingZero();
      sendSync();
      delayMicroseconds(REPEAT_GAP);
    }
    
    // Send final common message
    for (int j = 0; j < 8; j++) {
      sendByte(commonBytes[j]);
    }
    // Send final trailing zero for the last packet
    sendFinalTrailingZero();
    
    // Re-enable interrupts for the final gap
    interrupts();
    // Add clear final gap at end of transmission
    sendFinalGap();
  }
  
  Serial.println("Command sent (AC123-16D)");
}

// Parse hex string to byte array
void parseHex(String hexStr, byte* output, int len) {
  hexStr.replace(" ", "");
  for (int i = 0; i < len; i++) {
    output[i] = strtol(hexStr.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("RF Shade Transmitter v3.0 (AC123-06D Optimized)");
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);
  
  Serial.println("Commands:");
  Serial.println("TX:<prefix>,<header>,<id>,<cmd>,<type>,<common>,<isCC>,<cmdType> - Transmit command");
  Serial.println("INFO - Show status");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.startsWith("TX:")) {
      String data = input.substring(3);
      String parts[8]; // Now 8 parts to include cmdType
      int partCount = 0;
      int lastIndex = 0;
      for (int i = 0; i < data.length(); i++) {
        if (data[i] == ',') {
          parts[partCount++] = data.substring(lastIndex, i);
          lastIndex = i + 1;
        }
      }
      parts[partCount] = data.substring(lastIndex);
      
      if (partCount != 7) { // Check for 8 parts now
        Serial.println("Invalid TX command format");
        Serial.println("Expected: TX:<prefix>,<header>,<id>,<cmd>,<type>,<common>,<isCC>,<cmdType>");
        return;
      }
      
      byte prefix = strtol(parts[0].c_str(), NULL, 16);
      byte header[4];
      parseHex(parts[1], header, 4);
      byte id[2];
      parseHex(parts[2], id, 2);
      byte cmd[2];
      parseHex(parts[3], cmd, 2);
      byte remoteType = parts[4].toInt();
      byte commonByte = strtol(parts[5].c_str(), NULL, 16);
      bool isCC = parts[6].toInt() == 1;
      byte cmdType = parts[7].toInt(); // New parameter: 0=UP, 1=DOWN, 2=STOP
      
      // Transmit command once with improved structure
      transmitCommand(prefix, header, id, cmd, remoteType, commonByte, isCC, cmdType);
      
    } else if (input == "INFO") {
      Serial.println("RF Shade Transmitter v3.0 (AC123-06D Optimized) - Ready");
      Serial.println("Remote Types: 0=AC123-06D (FF prefix), 1=AC123-16D (FE prefix)");
      Serial.println("Command Types: 0=UP, 1=DOWN, 2=STOP");
    } else {
      Serial.println("Unknown command");
    }
  }
}
