# server side code

from flask import Flask
from flask import request

app = Flask(__name__)

# Variables to store last pulled values
last_temp = None
last_humidity = None

@app.route("/")
def hello():
    global last_temp, last_humidity

    # Get temperature and humidity from URL arguments
    temp = request.args.get('temperature')
    humidity = request.args.get('humidity')

    if temp is not None and humidity is not None:
        # If both are present in the URL, update the last values
        last_temp = temp
        last_humidity = humidity
        print(f"{'\033[0m'}Temp: {temp} C  /  Humidity: {humidity} % rH{'\033[32m'}")
        return f"We received temperature: {temp} and humidity: {humidity}"

    # If no URL parameters, use the last pulled values
    if last_temp is not None and last_humidity is not None:
        return f"No new data given. Prev Temperature: {last_temp} and Previous Humidity: {last_humidity}"

    # If no data at all, return an error message
    return "No temperature or humidity data available."