import socket
import appJar as aj
import subprocess

def start_server():
    global done
    done = False
    
    midi_file = app.getEntry("midi_file_entry")
    try:
        f = open(midi_file, mode="rb")
        data = f.read()
        f.close()
    except FileNotFoundError:
        print("MIDI file not found!")
        return

    print("Running TCP Socket server...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('0.0.0.0', 1235))
    s.listen(0)

    while True:
        client, addr = s.accept()
        print(f"Connection from {addr} has been established!")
        client.settimeout(5)
        while True:
            content = client.recv(1024)
            if len(content) == 0:
                break
            if str(content, 'utf-8') == '\r\n':
                continue
            elif str(content, 'utf-8') == 'GET\n\n':
                print(str(content, 'utf-8'))
                client.send(data)
                done = True
                continue
            elif str(content, 'utf-8') == 'PUT\n\n':
                print(str(content, 'utf-8'))
                with open("received_recording.mid", "wb") as received:
                    while True:
                        content = client.recv(1024)
                        if not content:
                            break
                        received.write(content)
        client.close()
        if done:
            s.close()
        break

def convert_to_midi():
    midi_file = app.getEntry("midi_file_entry")
    output_file = "output.mid"
    
    try:
        subprocess.run(["w2m", midi_file, output_file])
        print(f"File converted successfully: {output_file}")
    except FileNotFoundError:
        print("Error: w2m program not found!")
    except Exception as e:
        print(f"Error: {e}")

# Create GUI
app = aj.gui("Piano Hero Interface App", "400x350")
app.setSticky("ew")

# add logo
app.addImage("logo","logo.gif")
app.zoomImage("logo",-4)

# Add widgets
app.addLabel("title", "Enter MIDI file (skip.mid to skip):")
app.addEntry("midi_file_entry")
app.addButton("Start Server", start_server)
app.addButton("Convert to MIDI", convert_to_midi)

# Start GUI
app.go()
