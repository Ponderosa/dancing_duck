import argparse
import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np
from collections import deque
from scipy import stats


def main(device_id):
    # Initialize data lists with a maximum length
    max_points = 1000
    x_data, y_data, z_data = (
        deque(maxlen=max_points),
        deque(maxlen=max_points),
        deque(maxlen=max_points),
    )

    # Create figure and subplots
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Magnetometer Data")

    # Initialize scatter plots
    scatter1 = ax1.scatter([], [], c="r", label="Data")
    scatter2 = ax2.scatter([], [], c="g")
    scatter3 = ax3.scatter([], [], c="b")

    # Initialize center marker
    (center_marker,) = ax1.plot([], [], "y*", markersize=15, label="Estimated Center")

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

    # Add legend to the first plot
    ax1.legend()

    # Variables for center calculation
    center_x, center_y = 0, 0
    update_counter = 0

    # MQTT callback functions
    def on_connect(client, userdata, flags, rc, properties=None):
        print("Connected with result code " + str(rc))
        client.subscribe(f"dancing_duck/devices/{device_id}/sensor/mag")

    def on_message(client, userdata, msg):
        payload = msg.payload.decode()
        values = payload.split(", ")
        x = float(values[0].split(": ")[1])
        y = float(values[1].split(": ")[1])
        z = float(values[2].split(": ")[1])

        x_data.append(x)
        y_data.append(y)
        z_data.append(z)

    def fit_circle_with_confidence(x, y):
        x = np.array(x)
        y = np.array(y)

        # Compute means of x and y
        x_m = np.mean(x)
        y_m = np.mean(y)

        # Center the data
        u = x - x_m
        v = y - y_m

        # Compute sums
        Suv = np.sum(u * v)
        Suu = np.sum(u**2)
        Svv = np.sum(v**2)
        Suuv = np.sum(u**2 * v)
        Suvv = np.sum(u * v**2)
        Suuu = np.sum(u**3)
        Svvv = np.sum(v**3)

        # Solve the linear system
        A = np.array([[Suu, Suv], [Suv, Svv]])
        B = np.array([Suuu + Suvv, Svvv + Suuv]) / 2.0
        uc, vc = np.linalg.solve(A, B)

        # Compute center and radius
        xc = uc + x_m
        yc = vc + y_m
        R = np.sqrt(uc**2 + vc**2 + (Suu + Svv) / len(x))

        # Compute residuals
        residuals = np.abs(np.sqrt((x - xc) ** 2 + (y - yc) ** 2) - R)

        # Compute confidence measures
        rmse = np.sqrt(np.mean(residuals**2))
        max_error = np.max(residuals)

        # Compute R-squared
        ss_tot = np.sum(
            (
                np.sqrt((x - x_m) ** 2 + (y - y_m) ** 2)
                - np.mean(np.sqrt((x - x_m) ** 2 + (y - y_m) ** 2))
            )
            ** 2
        )
        ss_res = np.sum(residuals**2)
        r_squared = 1 - (ss_res / ss_tot)

        # Compute confidence intervals for center and radius
        n = len(x)
        standard_error = np.sqrt(
            np.sum(residuals**2) / (n - 3)
        )  # 3 parameters: xc, yc, R
        t_value = stats.t.ppf(0.975, n - 3)  # 95% confidence interval

        confidence_interval_center = t_value * standard_error * np.sqrt(2 / n)
        confidence_interval_radius = t_value * standard_error

        return (
            xc,
            yc,
            R,
            rmse,
            max_error,
            r_squared,
            confidence_interval_center,
            confidence_interval_radius,
        )

    def calculate_center_and_confidence():
        nonlocal center_x, center_y
        if len(x_data) > 3:  # Need at least 3 points to fit a circle
            center_x, center_y, R, rmse, max_error, r_squared, ci_center, ci_radius = (
                fit_circle_with_confidence(x_data, y_data)
            )
            return (
                center_x,
                center_y,
                R,
                rmse,
                max_error,
                r_squared,
                ci_center,
                ci_radius,
            )
        return center_x, center_y, None, None, None, None, None, None

    # Update function for animation
    def update(frame):
        nonlocal update_counter
        scatter1.set_offsets(np.c_[x_data, y_data])
        scatter2.set_offsets(np.c_[x_data, z_data])
        scatter3.set_offsets(np.c_[y_data, z_data])

        update_counter += 1
        if (
            update_counter % 50 == 0
        ):  # Update center and confidence measures every 50 frames
            center_x, center_y, R, rmse, max_error, r_squared, ci_center, ci_radius = (
                calculate_center_and_confidence()
            )
            print(
                f"Estimated circle center: ({center_x:.2f}, {center_y:.2f}) ± {ci_center:.2f}"
            )
            print(f"Estimated radius: {R:.2f} ± {ci_radius:.2f}")
            print(f"RMSE: {rmse:.4f}")
            print(f"Max error: {max_error:.4f}")
            print(f"R-squared: {r_squared:.4f}")
            print("-----")
            center_marker.set_data(
                [center_x], [center_y]
            )  # Pass as single-element lists

        return scatter1, scatter2, scatter3, center_marker

    # Function to handle window close event
    def on_close(event):
        client.loop_stop()
        plt.close(event.canvas.figure)

    # Set up MQTT client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    # Connect to MQTT broker
    client.connect("192.168.1.1", 1883, 60)

    # Start MQTT loop in a separate thread
    client.loop_start()

    # Set up animation
    ani = FuncAnimation(fig, update, interval=100, blit=True, cache_frame_data=False)

    # Connect the close event to the handler
    fig.canvas.mpl_connect("close_event", on_close)

    # Show the plot
    plt.tight_layout()
    plt.show()

    print("Script finished.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="A script that takes a device_id integer argument"
    )
    parser.add_argument("device_id", type=int, help="An integer argument for device_id")
    args = parser.parse_args()
    main(args.device_id)
