#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <map>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <vector>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>

//Serial.print("");
//Serial.println("");

WiFiMulti WiFiMulti;
std::string midi;

// Define custom functions for byte order conversion
unsigned int bigEndianToHost(unsigned char* buffer, int size) {
    unsigned int result = 0;
    for (int i = 0; i < size; i++) {
        result = (result << 8) | buffer[i];
    }
    return result;
}

unsigned short bigEndianToHostShort(unsigned char* buffer) {
    return (buffer[0] << 8) | buffer[1];
}

// Structure to store key signature data
struct KeySignature {
    char key;
    std::string scale;
    int ticks;
};

// Structure to store tempo data
struct Tempo {
    int bpm;
    int ticks;
};

// Structure to store time signature data
struct TimeSignature {
    int ticks;
    int numerator;
    int denominator;
    int measures;
};

// Structure to store control change data
struct ControlChange {
    int number;
    int ticks;
    int time;
    double value;
};

// Structure to store note data
struct Note {
    double duration = 0.0;
    int durationTicks;
    int midi;
    std::string name;
    int ticks;
    double velocity;
    int channel;
    std::string notetype;
    char binary;
};

// Structure to represent a track
struct Track {
    int channel;
    std::vector<ControlChange> controlChanges;
    std::vector<Note> notes;
    int endOfTrackTicks;
    long int tempoQuarterNote;
    std::string name;
    int thirtysecondNotesPerDivision;
    int maxTick = 0;
    std::map<int, std::bitset<88>> bitmap;
};

// Structure to store header information and tracks
struct MidiData {
    std::vector<KeySignature> keySignatures;
    std::vector<Tempo> tempos;
    std::vector<TimeSignature> timeSignatures;
    std::vector<Track> tracks;
    int division;
    int thirtysecondNotesPerDivision;
    std::map<int, std::bitset<88>> bitmap;
};

char noteOutput(Note note, bool hand);
bool readTrackChunk(std::istringstream& file, Track& track, unsigned short division);
void processMidiEvent(Track& track, int ticks, int channel, char statusByte, char dataByte1, char dataByte2, unsigned short division);
Note* findCorrespondingNoteOnEvent(Track& track, int note, int channel, int ticks);
std::string getNoteName(int midiNote);
int ReadVariableLengthValue(std::istringstream& file);

// Helper function to read a variable-length quantity from the stream
int ReadVariableLengthValue(std::istringstream& file) {
    int value = 0;
    int shift = 0;
    char byte;
    do {
        file.read(&byte, 1);
        value = (value << shift) | (byte & 0x7F);
        shift += 7;
    } while (byte & 0x80);
    return value;
}

std::string getNoteName(int midiNote) {
    const std::string pianoKeys[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    if (midiNote >= 0 && midiNote <= 127) {
        int octave = (midiNote / 12) - 1;
        int noteInOctave = midiNote % 12;

        return pianoKeys[noteInOctave] + std::to_string(octave);
    }
    else {
        return "Unknown";
    }
}

// Helper function to find the corresponding Note On event for a Note Off event
Note* findCorrespondingNoteOnEvent(Track& track, int note, int channel, int ticks) {
    for (Note& noteEvent : track.notes) {
        if (noteEvent.name == "Note On" && noteEvent.midi == note && noteEvent.ticks < ticks && noteEvent.channel == channel) {
            // Check if the Note On event has not been matched to a Note Off event
            if (noteEvent.duration == 0.0) {
                return &noteEvent;
            }
        }
    }
    return nullptr;
}





// Function to process MIDI events
void processMidiEvent(Track& track, int ticks, int channel, char statusByte, char dataByte1, char dataByte2, unsigned short division) {
    switch (statusByte & 0xF0) {
    case 0x80: // Note Off
            // Extract note and velocity information
    {
        int note = static_cast<int>(dataByte1);
        int velocity = static_cast<int>(dataByte2);

        // Find the corresponding Note On event
        Note* correspondingNoteOn = findCorrespondingNoteOnEvent(track, note, channel, ticks);

        if (correspondingNoteOn) {
            // Calculate the duration and update the Note On event
            double durationTicks = ticks - correspondingNoteOn->ticks;
            correspondingNoteOn->duration = (durationTicks / division);  // Convert ticks to seconds

            // Update the Note Off event
            Note noteOffEvent;
            noteOffEvent.durationTicks = ticks;
            noteOffEvent.midi = note;
            noteOffEvent.velocity = velocity;
            noteOffEvent.ticks = ticks;
            //noteOffEvent.duration = durationTicks / division;  // Convert ticks to seconds
            noteOffEvent.name = "Note Off";
            noteOffEvent.channel = channel;

            //track.notes.push_back(noteOffEvent);
        }
        // Map MIDI note to piano key
        std::string pianoKey = getNoteName(note);

        // Output to console
        //std::cout << "Note Off - Channel: " << channel << ", Note: " << note
          //  << " (" << pianoKey << "), Velocity: " << velocity << ", Ticks: " << ticks << std::endl;
    }
    break;
    case 0x90: // Note On
        // Extract note and velocity information
    {
        int note = static_cast<int>(dataByte1);
        int velocity = static_cast<int>(dataByte2);

        // Create a Note structure and add it to the track
        Note noteOnEvent;
        noteOnEvent.durationTicks = ticks;
        noteOnEvent.midi = note;
        noteOnEvent.velocity = velocity;
        noteOnEvent.ticks = ticks;
        noteOnEvent.duration = 0.0; // Duration for Note On is 0
        noteOnEvent.name = "Note On";
        noteOnEvent.channel = channel;
        if (noteOnEvent.velocity == 0)
        {
            Note* correspondingNoteOn = findCorrespondingNoteOnEvent(track, note, channel, ticks);

            if (correspondingNoteOn) {
                // Calculate the duration and update the Note On event
                double durationTicks = ticks - correspondingNoteOn->ticks;
                correspondingNoteOn->duration = (durationTicks / division);
                correspondingNoteOn->durationTicks = durationTicks;
                //std::cout << "Note off for " << correspondingNoteOn->midi << " for " << correspondingNoteOn->duration << std::endl;
            }

        }
        else {
            track.notes.push_back(noteOnEvent);

            // Map MIDI note to piano key
            std::string pianoKey = getNoteName(note);
            // When velocity is 0 set the duration for the previous note on of that type
            // Output to console
            //std::cout << "Note On - Channel: " << channel << ", Note: " << note
              //  << " (" << pianoKey << "), Velocity: " << velocity << ", Current Tick: " << ticks << std::endl;
        }
        break; }

    case 0xA0: // Aftertouch
        // Extract note and aftertouch pressure information
    {
        int note = static_cast<int>(dataByte1);
        int pressure = static_cast<int>(dataByte2);

        // Output to console (you can add logic to store this information if needed)
        //std::cout << "Aftertouch - Channel: " << channel << ", Note: " << note
          //  << ", Pressure: " << pressure << ",Current Ticks " << ticks << std::endl;
    }
    break;
    case 0xB0: // Control Change
        // Extract control change number and value information
    {
        int controlNumber = static_cast<int>(dataByte1);
        int controlValue = static_cast<int>(dataByte2);

        // Create a ControlChange structure and add it to the track
        ControlChange controlChangeEvent;
        controlChangeEvent.ticks = ticks;
        controlChangeEvent.number = controlNumber;
        controlChangeEvent.value = controlValue;
        track.controlChanges.push_back(controlChangeEvent);

        //std::cout << "Control Change - Channel: " << channel << ", Control Number: " << controlNumber
            //<< ", Value: " << controlValue << ", Current Tick: " << ticks << std::endl;
    }
    break;
    case 0xE0: // Pitch Bend
        // Extract pitch bend LSB and MSB
    {
        int lsb = static_cast<int>(dataByte1);
        int msb = static_cast<int>(dataByte2);

        // Combine LSB and MSB to get the 14-bit pitch bend value
        int pitchBendValue = (msb << 7) | lsb;

        // Output to console (you can add logic to store this information if needed)
        //std::cout << "Pitch Bend - Channel: " << channel << ", Value: " << pitchBendValue
           // << ",Current Tick: " << ticks << std::endl;
    }
    break;
    // Other cases can be added here for Midi Events
    default:
        // Unrecognized MIDI event, ignore it
        break;
    }
}


// Function to read the track chunk
bool readTrackChunk(std::istringstream& file, Track& track, unsigned short division) {
    char trackChunkID[4];
    file.read(trackChunkID, 4);

    if (std::string(trackChunkID, 4) != "MTrk") {
        std::cerr << "Invalid or missing track chunk in the MIDI file." << std::endl;
        return false;
    }

    // Read the track chunk size
    unsigned char trackChunkSizeBuffer[4];
    file.read(reinterpret_cast<char*>(trackChunkSizeBuffer), 4);
    int trackChunkSize = bigEndianToHost(trackChunkSizeBuffer, 4);

    // Store the starting position of the track data
    std::streampos trackStartPos = file.tellg();

    // Read track events
    int currentTick = 0;
    while (file && file.tellg() - trackStartPos < trackChunkSize) {
        int deltaTime = ReadVariableLengthValue(file);
        currentTick += deltaTime;

        char statusByte;
        file.read(&statusByte, 1);

        if (statusByte == static_cast<char>(0xFF)) {
            // Meta Event
            char metaType;
            file.read(&metaType, 1);

            int metaLength = ReadVariableLengthValue(file);

            // Handle meta events
            switch (metaType) {
            case 0x00: {
                // Sequence Number
                if (metaLength == 2) {
                    unsigned char sequenceNumberBuffer[2];
                    file.read(reinterpret_cast<char*>(sequenceNumberBuffer), 2);
                    int sequenceNumber = bigEndianToHostShort(sequenceNumberBuffer);
                    //std::cout << "Sequence Number: " << sequenceNumber << std::endl;
                }
                break;
            }
            case 0x01: {
                // Text Event
                std::vector<char> textEvent(metaLength);
                file.read(textEvent.data(), metaLength);
                textEvent.push_back('\0'); // Null-terminate the string
                //std::cout << "Text Event: " << textEvent.data() << std::endl;
                break;
            }
            case 0x02: {
                // Copyright Notice
                std::vector<char> copyrightNotice(metaLength);
                file.read(copyrightNotice.data(), metaLength);
                copyrightNotice.push_back('\0'); // Null-terminate the string
                //std::cout << "Copyright Notice: " << copyrightNotice.data() << std::endl;
                break;
            }
            case 0x03: {
                // Sequence/Track Name
                std::vector<char> sequenceName(metaLength);
                file.read(sequenceName.data(), metaLength);
                sequenceName.push_back('\0'); // Null-terminate the string
                track.name = sequenceName.data();
                Serial.print("Sequence/Track Name: "); 
                Serial.println(sequenceName.data());
                break;
            }
            case 0x04: {
                // Instrument Name
                std::vector<char> instrumentName(metaLength);
                file.read(instrumentName.data(), metaLength);
                instrumentName.push_back('\0'); // Null-terminate the string
                //std::cout << "Instrument Name: " << instrumentName.data() << std::endl;
                break;
            }
            case 0x20: {
                // MIDI Channel Prefix
                if (metaLength == 1) {
                    char channelPrefix;
                    file.read(&channelPrefix, 1);
                    int midiChannel = channelPrefix & 0x0F; // Extract the channel bits
                    //std::cout << "MIDI Channel Prefix: " << midiChannel << std::endl;
                }
                break;
            }
            case 0x2F: {
                // End of Track
                if (metaLength == 0) {
                    Serial.println("End of Track");
                    // You might want to set the endOfTrackTicks value in the Track struct here
                }
                break;
            }
            case 0x51: {
                // Set Tempo
                if (metaLength == 3) {
                    char tempoBytes[3];
                    file.read(tempoBytes, 3);
                    long int microsecondsPerQuarterNote = (tempoBytes[0] << 16) | (tempoBytes[1] << 8) | tempoBytes[2];
                    track.tempoQuarterNote = microsecondsPerQuarterNote;
                    Serial.print("Set Tempo: ");
                    Serial.print(microsecondsPerQuarterNote);
                    Serial.println(" microseconds per quarter note");
                }
                break;
            }
            case 0x58: {
                // Time Signature
                if (metaLength == 4) {
                    char timeSignatureBytes[4];
                    file.read(timeSignatureBytes, 4);
                    int nn = timeSignatureBytes[0]; // Numerator
                    int dd = timeSignatureBytes[1]; // Denominator
                    int cc = timeSignatureBytes[2]; // MIDI clocks per metronome click
                    int bb = timeSignatureBytes[3]; // Notated 32nd-notes per MIDI quarter note
                    //std::cout << "Time Signature: " << nn << "/" << (1 << dd) << ", MIDI clocks per metronome click: " << cc
                      //  << ", Notated 32nd-notes per MIDI quarter note: " << bb << std::endl;
                    track.thirtysecondNotesPerDivision = bb;
                }
                break;
            }
            case 0x59: {
                // Key Signature
                if (metaLength == 2) {
                    char keySignatureBytes[2];
                    file.read(keySignatureBytes, 2);
                    int sf = keySignatureBytes[0]; // Key Signature
                    int mi = keySignatureBytes[1]; // Minor Key
                    //std::cout << "Key Signature: " << sf << " " << (mi ? "Minor" : "Major") << std::endl;
                }
                break;
            }
                     // Handle other meta events as needed
            default:
                // Skip other meta events
                file.ignore(metaLength);
                break;
            }

        }
        else {
            // MIDI Event
            int midiChannel = (statusByte & 0x0F) + 1; // Extract the channel bits (1-16)

            switch (statusByte & 0xF0) {
            case 0x80: // Note Off
            case 0x90: // Note On
            case 0xA0: // Aftertouch
            case 0xB0: // Control Change
            case 0xE0: // Pitch Bend
                // These messages have two data bytes
                char dataByte1, dataByte2;
                file.read(&dataByte1, 1);
                file.read(&dataByte2, 1);

                // Process and store the MIDI event using the function
                processMidiEvent(track, currentTick, midiChannel, statusByte, dataByte1, dataByte2, division);
                break;
            case 0xC0: // Program change
                dataByte1;
                file.read(&dataByte1, 1);
                //Midi Event process 2
                break;
            default:
                // Unrecognized MIDI event, ignore it
                break;
            }
        }
    }

    return true;
}

char noteOutput(Note note, bool hand)
{
    char temp;
    int midimanipulated = note.midi - 21;
    temp = midimanipulated;
    if (hand) // if left hand is true 
    {
        temp = temp + 0x80;
    }
    return temp;
}
void recording(std::bitset<88> notesplayed[])
{
    File file = SPIFFS.open("/recording.mid","w");
    
    if(!file)
    {
      Serial.println("Failed to open file for writing");
      return;
    }
    else
    {
      Serial.println("File opened for writing");
    }
    //std::ofstream clearmidi("C:\\Users\\caden\\source\\repos\\testrecordingc++\\newFile.mid", std::ios::trunc);
    //clearmidi.close();
    //std::ofstream createdMidi("C:\\Users\\caden\\source\\repos\\testrecordingc++\\newFile.mid", std::ios::binary);
    //if (!createdMidi.is_open())
    //{
    //    return;
    //}
    
    // MIDI header
    const char header[] = { 'M','T','h','d', 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80 };


    // MIDI track header
    const char trackHeader[] = { 'M', 'T', 'r', 'k' };

    // MIDI events variables
    unsigned int deltaTime = 0; // Delta time
    unsigned int ticksPerQuarterNote = 120; // Ticks per quarter note
    unsigned int noteDuration = ticksPerQuarterNote; // Duration of each note (quarter note)
    std::vector<char> trackData; // Store MIDI events for this track
    // Iterate over each set of notes played
    // Initialize deltaTime outside the loop
    for (size_t i = 0; i < 20; ++i) {
        const auto& currentNotes = notesplayed[i];

        // Iterate through each bit in the bitset
        for (size_t j = 0; j < currentNotes.size(); ++j) {
            // Check if the note is turned on (0 to 1 transition)
            if (i == 0)
            {
                if (currentNotes[j])
                {
                    trackData.push_back(0x00);
                    trackData.push_back(0x90); // Note-on status byte
                    trackData.push_back(j + 20); // Note number
                    trackData.push_back(0x7F); // Velocity (max velocity)
                }

            }
            else if (currentNotes[j] && (!notesplayed[i - 1][j])) {
                // Write MIDI note-on event with current deltaTime
                // Calculate VLQ for delta time
                std::vector<unsigned char> vlqBytes;
                unsigned int deltaTimeValue = deltaTime;
                do {
                    unsigned char byte = deltaTimeValue & 0x7F; // Extract lowest 7 bits
                    deltaTimeValue >>= 7; // Shift right by 7 bits
                    if (deltaTimeValue > 0) {
                        byte |= 0x80; // Set MSB if more bytes follow
                    }
                    vlqBytes.push_back(byte);
                } while (deltaTimeValue > 0);

                // Write VLQ bytes to trackData
                for (auto it = vlqBytes.rbegin(); it != vlqBytes.rend(); ++it) {
                    trackData.push_back(*it);
                }
                trackData.push_back(0x90); // Note-on status byte
                trackData.push_back(j + 20); // Note number
                trackData.push_back(0x7F); // Velocity (max velocity)
            }
            // Check if the note is turned off (1 to 0 transition)
            else if (!currentNotes[j] && (notesplayed[i - 1][j])) 
            {
                // Write MIDI note-off event with current deltaTime
                // Calculate VLQ for delta time
                std::vector<unsigned char> vlqBytes;
                unsigned int deltaTimeValue = deltaTime;
                do {
                    unsigned char byte = deltaTimeValue & 0x7F; // Extract lowest 7 bits
                    deltaTimeValue >>= 7; // Shift right by 7 bits
                    if (deltaTimeValue > 0) {
                        byte |= 0x80; // Set MSB if more bytes follow
                    }
                    vlqBytes.push_back(byte);
                } while (deltaTimeValue > 0);

                // Write VLQ bytes to trackData
                for (auto it = vlqBytes.rbegin(); it != vlqBytes.rend(); ++it) {
                    trackData.push_back(*it);
                }
                trackData.push_back(0x80); // Note-off status byte
                trackData.push_back(j + 20); // Note number
                trackData.push_back(0x00); // Velocity (note-off)
            }
        }
        
        // Increment deltaTime by ticksPerQuarterNote for the next iteration
        deltaTime = 120;
    }


    file.write((uint8_t*)header, sizeof(header));
    //Write MIDI track header
    file.write((uint8_t*)trackHeader, sizeof(trackHeader));
    //createdMidi.write(header, sizeof(header));
    // Write MIDI track header
    //createdMidi.write(trackHeader, sizeof(trackHeader));

    // Write track length
    unsigned int trackLength = trackData.size();

    uint8_t temp = trackLength >> 24;
    file.write(&temp, sizeof(temp));
    temp = trackLength >> 16;
    file.write(&temp, sizeof(temp));
    temp = trackLength >> 8;
    file.write(&temp, sizeof(temp));
    temp = trackLength;
    file.write(&temp, sizeof(temp));

    //createdMidi.put(trackLength >> 24);
    //createdMidi.put(trackLength >> 16);
    //createdMidi.put(trackLength >> 8);
    //createdMidi.put(trackLength);

    // Write track data
    file.write((uint8_t*)trackData.data(), trackData.size());
    //createdMidi.write(trackData.data(), trackData.size());

    // Close the MIDI file
    file.close();
    //createdMidi.close();
}
#define LED_BUILTIN 2
#define RECORDING_MODE 0
#define PLAYBACK_MODE 1


//bens code
static const long long int spiClk = 1000000;  // 1 MHz
SPIClass* vspi = NULL;

int note_bytes[11] = {0};
unsigned long previous_time = 0;

#define DELAY 2000  //general delay used between writes to a row of shift registers
#define ShiftRegNum 11 //number of shift registers to test
#define Rows 4        //how many rows of LEDs
#define LED 2         //pin for LED on board of ESP32 (for debugging purposes)
//these are all arbitrary values
//Cadens code should write into the 'next' array with bits and it should cycle through from there
//for PIC24
int next[11] = { 0};
int  first[11] = { 0 }; //initial first row
int  second[11] = { 0 };//initial second row
int third[11] = { 0 };//initial third row
int  fourth[11] = { 0 };
//for ESP32
// int next =    0b111111111111;
// int first =   0b100011000000; //initial first row
// int second=   0b010000110000;//initial second row
// int third =   0b001000001100;//initial third row
// int fourth =  0b000100000011;//initial fourth row
// int hold =    0b000000000000;
//just used to cycle through these arrays to show movement and working ^^^

//used to 'push down' arrays and fill the next one with incoming new data
void cycle() 
{
    for (int i = 0; i < ShiftRegNum; i++) 
    {
        fourth[i] = third[i];
        third[i] = second[i];
        second[i] = first[i];
        first[i] = next[i];
        next[i] =note_bytes[i];
    }
    //next = Cadens data!!!! (these may need to be floats or broken back up into an array)
}

//if desired to show static LED arrays (spelling a word or something)
void static_display(int array[Rows][ShiftRegNum]) 
{
  digitalWrite(4, LOW); //reset pin of decade counter low

  for (int r = 0; r < Rows; r++)
  {
    //for 'SPIsettings' spiClk speed is prev declared, MSBFIRST is most sig bit is transmitted first
    //SPI_MODE can still be looked at, might be 2 instead of 0???
      vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0)); //begins spi transaction
      //for driving latch low for shift register and counter low for the decade counter
      digitalWrite(vspi->pinSS(), LOW);  //pull SS low to prep other end for transfer
    
    for(int c = 0; c < ShiftRegNum; c++)
    {
      //for transferring the desired data
      vspi->transfer(array[r][c]);
      //
    } 

    //every high signal latches the shift register data and counts the decade counter to the next row of LEDs
      digitalWrite(vspi->pinSS(), HIGH);  //pull ss high to signify end of data transfer
      vspi->endTransaction(); //ends SPI transaction
      //delay(1000); //this delay can be increased a lot to show individual rows cycling through
  }

  digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
  //delay(10);
}

//for 'waterfall' effect (what we will be using for the learning experience)
void waterfall_display() 
{
  //std::bitset<8> array[Rows][ShiftRegNum];
  //after every cycle function this will be updated to next iteration of notes
    int array[Rows][ShiftRegNum];

         for (int k = 0; k<ShiftRegNum ; k++) 
       {
        array[0][k] = first[k];
        array[1][k] = second[k];
        array[2][k] = third[k];
        array[3][k] = fourth[k];
       }

  digitalWrite(4, LOW); //reset pin of decade counter low

  for (int r = 0; r < Rows; r++)
  {
    //for 'SPIsettings' spiClk speed is prev declared, MSBFIRST is most sig bit is transmitted first
    //SPI_MODE can still be looked at, might be 2 instead of 0???
      vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0)); //begins spi transaction
      //for driving latch low for shift register and counter low for the decade counter
      digitalWrite(vspi->pinSS(), LOW);  //pull SS low to prep other end for transfer
    
    for(int c = 8; c > 2; c--)
    {
      //for transferring the desired data
      vspi->transfer(array[r][c]);
      //
    } 

    //every high signal latches the shift register data and counts the decade counter to the next row of LEDs
      digitalWrite(vspi->pinSS(), HIGH);  //pull ss high to signify end of data transfer
      vspi->endTransaction(); //ends SPI transaction
      //delay(1000); //this delay can be increased a lot to show individual rows cycling through
  }

  //digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
  //delay(10);
}


//Ron code

const int clock_pin = 13;
const int clock_inhibit = 14;
const int shift_load = 15;
const int q_h = 16;
const long shift_interval = 1000;  // Function will be called once a millisecond
int i = 0;
int k = 0;
std::bitset<8> keyPress = 0;
std::bitset<8> shift_register_1 = 0;
std::bitset<8> shift_register_2 = 0;
std::bitset<8> shift_register_3 = 0;
std::bitset<8> shift_register_4 = 0;
std::bitset<8> shift_register_5 = 0;
std::bitset<8> shift_register_6 = 0;
std::bitset<8> shift_register_7 = 0;
std::bitset<8> shift_register_8 = 0;
std::bitset<8> shift_register_9 = 0;
std::bitset<8> shift_register_10 = 0;
std::bitset<8> shift_register_11 = 0;
std::bitset<88> output;


std::bitset<8> receiveSR(void) {
    std::bitset<8> shift_register = {};

    for (i = 0; i < 8; i++) {
        if (i < 3)
        {}
        else if (digitalRead(q_h) == HIGH) {
            shift_register.set(i);  // Load output into variable
        }
        else {
            shift_register.reset(i);  // Load output into variable
        }


        // Clock toggle
        digitalWrite(clock_pin, HIGH);
        digitalWrite(clock_pin, LOW);
    }

    return shift_register;
}
void readSR(void) {
    output.reset();
    shift_register_1.reset();

    digitalWrite(shift_load, LOW); // Load the registers
    digitalWrite(shift_load, HIGH); // Prepare to shift
    digitalWrite(clock_inhibit, LOW); // Clock can now be used

    shift_register_1 = receiveSR();
          for(int i = 3; i < 8; i++)
          {
              // Set the bit in the byte
              if (shift_register_1[i]) 
              {
                  output.set(i+25); // Set the bit to 1
                  Serial.print("X");
              } 
              else 
              {
                  output.reset(i+25); // Set the bit to 0 (optional, already initialized to 0)
                  Serial.print(".");
              }
          }
          Serial.println();
    
    

    digitalWrite(clock_inhibit, HIGH); // Disable the clock

    return;
}

unsigned long past_time = 0;
unsigned long p_time = 0;

int idle_next[7] =   { 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000 };
int idle_first[7] =  { 0b00110000, 0b00110000, 0b00110000, 0b00110000, 0b00110000, 0b00110000, 0b00110000 }; //initial first row
int idle_second[7] = { 0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100 };//initial second row
int idle_third[7] =  { 0b00110000, 0b00110000, 0b00110000, 0b00110000, 0b00110000, 0b00110000, 0b00110000 };//initial third row
int idle_fourth[7] = { 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000 };//initial fourth row
int idle_hold[7] =   { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 };

void idle_waterfall_display() 
{
  int array[Rows][7];
  //after every cycle function this will be updated to next iteration of notes

for (int k = 6; k > -1 ; k--) 
       {
        array[0][k] = idle_first[k];
        array[1][k] = idle_second[k];
        array[2][k] = idle_third[k];
        array[3][k] = idle_fourth[k];
       }

  digitalWrite(4, LOW); //reset pin of decade counter low

  for (int r = 0; r < Rows; r++)
  {
    //for 'SPIsettings' spiClk speed is prev declared, MSBFIRST is most sig bit is transmitted first
    //SPI_MODE can still be looked at, might be 2 instead of 0???
      vspi->beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE0)); //begins spi transaction
      //for driving latch low for shift register and counter low for the decade counter
      digitalWrite(vspi->pinSS(), LOW);  //pull SS low to prep other end for transfer
    
    for(int c = 6; c > -1; c--)
    {
      //for transferring the desired data
      vspi->transfer(array[r][c]);
      //
    } 

    //every high signal latches the shift register data and counts the decade counter to the next row of LEDs
      digitalWrite(vspi->pinSS(), HIGH);  //pull ss high to signify end of data transfer
      vspi->endTransaction(); //ends SPI transaction
      //delay(100); //this delay can be increased a lot to show individual rows cycling through
  }

  unsigned long now_time = millis();

  if (now_time - past_time >= 300)
  {
    digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
    idle_cycle(); //cycles through notes
    //delay(200); //eventually remove just a debounce for proof of concept

    past_time = now_time;
  }
  else
  {
    digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
  //delay(10);
  }

  //digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
  //delay(10);
}

void idle_cycle() 
{
    for (int i = 0; i < ShiftRegNum; i++) 
    {
        idle_hold[i] = idle_fourth[i];
        idle_fourth[i] = idle_third[i];
        idle_third[i] = idle_second[i];
        idle_second[i] = idle_first[i];
        idle_first[i] = idle_next[i];
        idle_next[i] = idle_hold[i];
    }
    }



//#define DELAY 300  //general delay used between writes to a row of shift registers

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    pinMode(clock_pin, OUTPUT);
    pinMode(clock_inhibit, OUTPUT);
    pinMode(shift_load, OUTPUT);
    pinMode(q_h, INPUT);
    // initialize built-in LED pin as an output.
    pinMode(LED, OUTPUT);
    pinMode(4, OUTPUT);   //pin to reset decade counter so there is no weird delay
    pinMode(0, INPUT);
    
    //finishing setting up vspi
    vspi = new SPIClass(VSPI);
    vspi->begin();
    pinMode(vspi->pinSS(), OUTPUT);

    if(SPIFFS.begin(true))
    {
      Serial.println("SPIFFS filesystem mounted successfully");
      if(SPIFFS.exists("/recording.mid")) Serial.println("A recording exists in SPIFFS filesystem!");
      if(SPIFFS.exists("/demo.mid")) Serial.println("A demo MIDI exists in SPIFFS filesystem!");
    }
    else 
    {
      Serial.println("ERROR: SPIFFS filesystem mount failed!");
    }
    delay(10);

    // We start by connecting to a WiFi network
    WiFiMulti.addAP("Kirby", "8675309bro");

    Serial.println();
    Serial.println();
    Serial.print("Waiting for WiFi... ");

    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(500);
}

unsigned timeout = 0;
void loop() {
    const uint16_t port = 1235;
    const char * host = "172.20.10.2"; // ip or dns
    int mode = PLAYBACK_MODE;
    int maxloops = 0;

    Serial.print("Connecting to host machine ");
    Serial.println(host);
    pinMode(LED_BUILTIN, HIGH);
    // Use WiFiClient class to create TCP connections
    WiFiClient client;

    File readFile = SPIFFS.open("/recording.mid","r");
   

    if (!client.connect(host, port)) {
        //pinMode(LED_BUILTIN, LOW);
        //Serial.println("Connection to host failed.");
        //Serial.println("Waiting 5 seconds before retrying...");
        //delay(5000);

    unsigned long n_time = millis();

      if (n_time - p_time >= 5000)
      {
        pinMode(LED_BUILTIN, LOW);
        Serial.println("Connection to host failed.");
        Serial.println("Waiting 5 seconds before retrying...");
        p_time = n_time;
      }
      else
      {
          idle_waterfall_display(); 
      }
        return;
    }

    // This will send a request to the server
    //uncomment this line to send an arbitrary string to the server
    //client.print("Send this data to the server");
    //uncomment this line to send a basic document request to the server
    
    client.print("GET\n\n");

  //wait for the server's reply to become available
  while (!client.available() && maxloops < 1000)
  {
    maxloops++;
    delay(1); //delay 1 msec
  }

  while (client.available() > 0 && mode == PLAYBACK_MODE)
  {
        //read back one line from the server
        char c = client.read();
        midi.push_back(c);
        
        Serial.print(c);
  }



if(mode == PLAYBACK_MODE)
{
    // Open the MIDI file
    std::istringstream midiFile(midi, std::ios::binary);

    if (!midiFile) 
    {
        std::cerr << "Error opening the MIDI file." << std::endl;
        return;
    }

    // Read the header chunk
    char headerChunkID[4];
    midiFile.read(headerChunkID, 4);
    if(std::string(headerChunkID, 4) == "SKIP")
    {
        pinMode(LED_BUILTIN, LOW);
        std::bitset<88> bitsets[20]{};
        bitsets[0][40]=  1;
        bitsets[1][40] = 0; 
        bitsets[2][41] = 1;
        bitsets[3][41] = 0; 
        bitsets[4][42] = 1;
        bitsets[5][42] = 0; 
        bitsets[6][43] = 1;
        bitsets[7][43] = 0; 
        bitsets[8][44] = 1;
        bitsets[9][44] = 0; 
        bitsets[10][45] = 1;
        bitsets[11][45] = 0; 
        bitsets[12][46] = 1;
        bitsets[13][46] = 0;
        bitsets[14][47] = 1;
        bitsets[15][47] = 0;
        bitsets[16][48] = 1;
        bitsets[17][48] = 0;
        bitsets[18][48] = 1;
        bitsets[19][48] = 0;
        recording(bitsets);
        client.print("PUT\n\n");
        while(readFile.available())
        {
            char byte = readFile.read();
            client.write(byte);
        }
        Serial.println("Closing connection.");
        client.stop();
        while(true);
    }
        Serial.println("Closing connection.");
        client.stop();

    if (std::string(headerChunkID, 4) != "MThd") 
    {
        std::cerr << "Invalid MIDI file format." << std::endl;
        return;
    }

    MidiData midiData;

    unsigned char headerChunkSizeBuffer[4];
    midiFile.read(reinterpret_cast<char*>(headerChunkSizeBuffer), 4);
    unsigned int headerChunkSize = bigEndianToHost(headerChunkSizeBuffer, 4);

    unsigned char formatTypeBuffer[2];
    midiFile.read(reinterpret_cast<char*>(formatTypeBuffer), 2);
    unsigned short formatType = bigEndianToHostShort(formatTypeBuffer);

    unsigned char numTracksBuffer[2];
    midiFile.read(reinterpret_cast<char*>(numTracksBuffer), 2);
    unsigned short numTracks = bigEndianToHostShort(numTracksBuffer);

    unsigned char divisionBuffer[2];
    midiFile.read(reinterpret_cast<char*>(divisionBuffer), 2);
    unsigned short division = bigEndianToHostShort(divisionBuffer);

    std::cout << "Format Type: " << formatType << std::endl;
    std::cout << "Number of Tracks: " << numTracks << std::endl;
    std::cout << "Division: " << division << std::endl;
    midiData.division = division;



    for (int trackNumber = 0; trackNumber < numTracks; ++trackNumber) 
    {
        Track track;
        if (!readTrackChunk(midiFile, track, division)) 
        {
            return;
        }

        // Add the track to the MidiData structure
        midiData.tracks.push_back(track);
    }
    for (int i = 0; i < midiData.tracks.size(); i++)
    {

        int notesize = midiData.tracks[i].notes.size();
        for (int noteindex = 0; noteindex < notesize; noteindex++)
        {
            if (midiData.tracks[i].maxTick < midiData.tracks[i].notes[noteindex].ticks)
            {
                midiData.tracks[i].maxTick = midiData.tracks[i].notes[noteindex].ticks;
            }
        }
        int counter = midiData.division / midiData.tracks[i].thirtysecondNotesPerDivision;
        for (int count = 0; count < midiData.tracks[i].maxTick; count = count + counter)
        {
            midiData.tracks[i].bitmap[count] = std::move(std::bitset<88>());
        }
        for (int noteindex = 0; noteindex < notesize; noteindex++)
        {
            int tick = midiData.tracks[i].notes[noteindex].ticks; // Get the tick count
            int midiNote = midiData.tracks[i].notes[noteindex].midi; // Get the MIDI note

            // Check if the tick count entry exists in the bitmap
            if (midiData.tracks[i].bitmap.find(tick) != midiData.tracks[i].bitmap.end()) {
                // Access the bit vector associated with the tick count
                std::bitset<88>& bitVector = midiData.tracks[i].bitmap[tick];
                bitVector[midiNote] = true;
            }
        }
       /*for (int index = 0; index < midiData.tracks[i].bitmap.size(); index++)
        {
            std::bitset<88>& bitVector = midiData.tracks[i].bitmap[index * counter];
            for(int count = 0; count < 88; count++)
            {
                if (bitVector[count] == false)
                {
                    std::cout << "0";
                }
                else if (bitVector[count] == true)
                {
                    std::cout << "1";
                }
            }
            std::cout << std::endl;
            //std::cin.ignore(); // Ignore any previous input
            //std::cin.get(); // Wait for a key press
        }*/
    }

    int counter = midiData.division / midiData.tracks[1].thirtysecondNotesPerDivision;
    if (midiData.tracks.size() > 2)
    {
        for (int count = 0; count < midiData.tracks[1].maxTick; count = count + counter)
        {
            midiData.bitmap[count] = std::move(std::bitset<88>());
        }
        for (int index = 0; index < midiData.tracks[1].bitmap.size(); index++)
        {
            std::bitset<88>& bitVector = midiData.tracks[1].bitmap[index * counter];
            std::bitset<88>& bitVector2 = midiData.tracks[2].bitmap[index * counter];
            std::bitset<88>& midibitvector = midiData.bitmap[index * counter];
            for (int count = 0; count < 88; count++)
            {
                midibitvector[count] = (bitVector[count] || bitVector2[count]);
            }
            Serial.print("bitmap made");

        }
    }
    if (midiData.tracks.size() == 2)
    {
        for (int count = 0; count < midiData.tracks[1].maxTick; count = count + counter)
        {
            midiData.bitmap[count] = std::move(std::bitset<88>());
        }
        for (int index = 0; index < midiData.tracks[1].bitmap.size(); index++)
        {
            std::bitset<88>& bitVector = midiData.tracks[1].bitmap[index * counter];
            std::bitset<88>& midibitvector = midiData.bitmap[index * counter];
         //std::cin.ignore(); // Ignore any previous input
         //std::cin.get(); // Wait for a key press
            for (int count = 0; count < 88; count++)
            {
                midibitvector[count] = (bitVector[count]);
            }
        }
    }
    
    for (int index = 0; index < midiData.bitmap.size()+3; index++)
    {
      std::bitset<88> bitVector;
      if(index < midiData.bitmap.size())
      {
         bitVector = midiData.bitmap[index * counter];
      }
      else
      {
        bitVector.reset();
      }
        bool fl = false;
        for (int count = 0; count < 88; count++)
        {
            if (bitVector[count] == true) 
            {
                      fl = true;
            }
        }
        if ((fl == false) && !(midiData.bitmap.size() <= index))
        {
            continue;
        }

        

        //waterfall_display(bitVector);
        for (int count = 0; count < 88; count++)
        {
              int byteIndex = count / 8; // Calculate the index of the byte in thenote_bytes array
              int bitIndex = count % 8;  // Calculate the index of the bit within the byte

              // Set the bit in the byte
              if (bitVector[count]) 
              {
                 note_bytes[byteIndex] |= (1 << bitIndex); // Set the bit to 1
                  Serial.print("X");
              } 
              else 
              {
                 note_bytes[byteIndex] &= ~(1 << bitIndex); // Set the bit to 0 (optional, already initialized to 0)
                  Serial.print(".");
              }
          }

          // Output the 11 8-bit array elements
  for (int i = 0; i < 11; i++) {
      Serial.print(note_bytes[i], BIN); // Output in binary format
      Serial.print(" "); // Separate bytes with a space
  }

          bool flag = 1;
          while(flag == 1)
          {
            waterfall_display();


            unsigned long current_time = millis();

            if (current_time - previous_time >= 1000)
              {
              digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
              cycle(); //cycles through notes
              //delay(200); //eventually remove just a debounce for proof of concept
              flag = 0;
              previous_time = current_time;
            }
            else
            {
              digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
            //delay(10);
            }
/*
            if(digitalRead(0) == 0) //if 'correct' buttons are pressed example
            {
              digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
              cycle(); //cycles through notes
              delay(200); //eventually remove just a debounce for proof of concept
              flag = 0;
            }
            else
            {
              digitalWrite(4, HIGH); //reset pin of decade counter high to reset the counter to remove the delay
            //delay(10);
            }
          }
*/          
          }
        Serial.println("line end");
        //std::cin.ignore(); // Ignore any previous input
        //std::cin.get(); // Wait for a key press
    }
}
    /*
    Store in the cache
    */
END:
    while(true){}
  }