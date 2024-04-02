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
    std::map<int, std::vector<bool>> bitmap;
};

// Structure to store header information and tracks
struct MidiData {
    std::vector<KeySignature> keySignatures;
    std::vector<Tempo> tempos;
    std::vector<TimeSignature> timeSignatures;
    std::vector<Track> tracks;
    int division;
    int thirtysecondNotesPerDivision;
    std::map<int, std::vector<bool>> bitmap;
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

#define LED_BUILTIN 2
void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
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

void loop() {
    const uint16_t port = 1235;
    const char * host = "172.20.10.2"; // ip or dns

    Serial.print("Connecting to host machine ");
    Serial.println(host);
    pinMode(LED_BUILTIN, HIGH);
    // Use WiFiClient class to create TCP connections
    WiFiClient client;

    if (!client.connect(host, port)) {
        pinMode(LED_BUILTIN, LOW);
        Serial.println("Connection to host failed.");
        Serial.println("Waiting 5 seconds before retrying...");
        delay(5000);
        return;
    }

    // This will send a request to the server
    //uncomment this line to send an arbitrary string to the server
    //client.print("Send this data to the server");
    //uncomment this line to send a basic document request to the server
    client.print("GET /index.html HTTP/1.1\n\n");

  int maxloops = 0;

  //wait for the server's reply to become available
  while (!client.available() && maxloops < 1000)
  {
    maxloops++;
    delay(1); //delay 1 msec
  }
  while (client.available() > 0)
  {
    //read back one line from the server
    char c = client.read();
    midi.push_back(c);
    Serial.print(c);
  }
    Serial.println("Closing connection.");
    client.stop();

    pinMode(LED_BUILTIN, LOW);

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
            midiData.tracks[i].bitmap[count] = std::vector<bool>(88, false);
        }
        for (int noteindex = 0; noteindex < notesize; noteindex++)
        {
            int tick = midiData.tracks[i].notes[noteindex].ticks; // Get the tick count
            int midiNote = midiData.tracks[i].notes[noteindex].midi; // Get the MIDI note

            // Check if the tick count entry exists in the bitmap
            if (midiData.tracks[i].bitmap.find(tick) != midiData.tracks[i].bitmap.end()) {
                // Access the bit vector associated with the tick count
                std::vector<bool>& bitVector = midiData.tracks[i].bitmap[tick];
                bitVector[midiNote] = true;
            }
        }
       /*for (int index = 0; index < midiData.tracks[i].bitmap.size(); index++)
        {
            std::vector<bool>& bitVector = midiData.tracks[i].bitmap[index * counter];
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
            midiData.bitmap[count] = std::vector<bool>(88, false);
        }
        for (int index = 0; index < midiData.tracks[1].bitmap.size(); index++)
        {
            std::vector<bool>& bitVector = midiData.tracks[1].bitmap[index * counter];
            std::vector<bool>& bitVector2 = midiData.tracks[2].bitmap[index * counter];
            std::vector<bool>& midibitvector = midiData.bitmap[index * counter];
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
            midiData.bitmap[count] = std::vector<bool>(88, false);
        }
        for (int index = 0; index < midiData.tracks[1].bitmap.size(); index++)
        {
            std::vector<bool>& bitVector = midiData.tracks[1].bitmap[index * counter];
            std::vector<bool>& midibitvector = midiData.bitmap[index * counter];
         //std::cin.ignore(); // Ignore any previous input
         //std::cin.get(); // Wait for a key press
            for (int count = 0; count < 88; count++)
            {
                midibitvector[count] = (bitVector[count]);
            }
        }
    }
    
    for (int index = 0; index < midiData.bitmap.size(); index++)
    {
        std::vector<bool>& bitVector = midiData.bitmap[index * counter];
        for (int count = 0; count < 88; count++)
        {
            if (bitVector[count] == false)
            {
                Serial.print("0");
            }
            else if (bitVector[count] == true)
            {
                Serial.print("1");
            }
        }
        Serial.println("line end");
        //std::cin.ignore(); // Ignore any previous input
        //std::cin.get(); // Wait for a key press
    }

    /*
    Store in the cache
    */
    while(true){}
  }
