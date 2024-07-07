import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np

# Initialize data lists
x_data, y_data, z_data = [], [], []

# Create figure and subplots
fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(15, 5))
fig.suptitle("Magnetometer Data")

# Initialize scatter plots
scatter1 = ax1.scatter([], [], c='r')
scatter2 = ax2.scatter([], [], c='g')
scatter3 = ax3.scatter([], [], c='b')

# Set fixed limits for all axes
for ax in (ax1, ax2, ax3):
    ax.set_xlim(-100, 100)
    ax.set_ylim(-100, 100)

ax1.set_xlabel("X")
ax1.set_ylabel("Y")
ax2.set_xlabel("X")
ax2.set_ylabel("Z")
ax3.set_xlabel("Y")
ax3.set_ylabel("Z")

# MQTT callback functions
def on_connect(client, userdata, flags, rc, properties=None):
    print("Connected with result code "+str(rc))
    client.subscribe("dancing_duck/devices/1/sensor/mag")

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    values = payload.split(", ")
    x = float(values[0].split(": ")[1])
    y = float(values[1].split(": ")[1])
    z = float(values[2].split(": ")[1])
    
    x_data.append(x)
    y_data.append(y)
    z_data.append(z)
    
# Update function for animation
def update(frame):
    scatter1.set_offsets(np.c_[x_data, y_data])
    scatter2.set_offsets(np.c_[x_data, z_data])
    scatter3.set_offsets(np.c_[y_data, z_data])
    
    return scatter1, scatter2, scatter3

# Function to handle window close event
def on_close(event):
    client.loop_stop()
    plt.close(event.canvas.figure)

# Set up MQTT client
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

# Connect to MQTT broker
client.connect("192.168.5.6", 1883, 60)

# Start MQTT loop in a separate thread
client.loop_start()

# Set up animation
ani = FuncAnimation(fig, update, interval=100, blit=True, cache_frame_data=False)

# Connect the close event to the handler
fig.canvas.mpl_connect('close_event', on_close)

# Show the plot
plt.tight_layout()
plt.show()

print("Script finished.")