# libs for handling flask serverr
from flask import Flask, request, render_template
from datetime import datetime

# libs for handling speed graph
import os
import shutil
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.style as style

# lib for handling mobile notifications
from pushbullet import Pushbullet

# takes in API token retrieved on PushBullet site settings after login
pb = Pushbullet('REDACTED')

# stores speed/time data for producing graph on dashboard
df = pd.DataFrame(columns=["time", "speed"])

app = Flask(__name__)

# Variables to store last pulled values
last_speed = None
notified = False

# maintains status of dash lights as it's received 
dash_lights = {
    "engine_light": "OFF",
    "tire_light": "OFF",
    "oil_light": "OFF",
    "battery_light": "OFF"
}

# never used; tracks the last time each dash light status changed
dash_update_stamp = {
    "engine_light": "Unknown",
    "tire_light": "Unknown",
    "oil_light": "Unknown",
    "battery_light": "Unknown"
}

# produces the speed/time graph for the dashboard (saves it as png on server side, which is later pulled by HTML)
def save_speed_plot(df):
    style.use('dark_background')

    plt.figure()
    plt.plot(df["time"], df["speed"], marker='o')
    plt.xticks(rotation=45)
    plt.tight_layout()

    ax = plt.gca()
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%I:%M %p'))

    temp_path = "temp_plot.png"
    destination_folder = "static/generated"
    destination_file = os.path.join(destination_folder, "speed_plot.png")

    plt.savefig(temp_path)
    plt.close()

    os.makedirs(destination_folder, exist_ok=True)
    shutil.move(temp_path, destination_file)

# route for handling data input
@app.route("/")
def update():
    # values that should be updated globally, mainly df which is dataframe updating speed/time data
    global last_speed, df, notified

    data = request.args.to_dict()

    if len(data) == 0:
        return "Error: No arguments found."

    last_speed = float(request.args.get('speed'))

    # sends notification if user goes over 70
    if (last_speed >= 70 and not notified):
        notified = True
        pb.push_note("Vehicle Notification", "The driver is speeding over 70 mph")

    # ensures that notification is only sent once when driver passes 70, not on every iteration
    if (notified and last_speed < 70):
        notified = False

    new_row = pd.DataFrame({
        "time": [datetime.utcnow()],
        "speed": [last_speed]
    })

    df = pd.concat([df, new_row], ignore_index=True)

    # goes through the dashlight data if sent by Devkit V1
    for key in dash_lights.keys():
        if key in data:
            dash_lights[key] = data[key]
            # currently left in UTC format
            dash_update_stamp[key] = datetime.utcnow().strftime("%m/%d/%y %I:%M %p UTC")

            # send mobile notification updating car owner about dash light turning on/off
            pb.push_note("Vehicle Notification", f"{key} is now {data[key]}.")

    return "Data has been successfully updated"

# route for serving dashboard page
@app.route("/serve")
def serve():
    # generates new speed/time graph and saves it as png
    save_speed_plot(df)

    # serves client with HTML page which implements variables for properly correlating each dash light to on/off
    return render_template('content.html',
                           engine_light=dash_lights['engine_light'],
                           tire_light=dash_lights['tire_light'],
                           oil_light=dash_lights['oil_light'],
                           battery_light=dash_lights['battery_light'])