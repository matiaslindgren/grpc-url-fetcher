import flask

app = flask.Flask(__name__)

@app.route("/echo/<string:message>")
def echo(message):
    return message

@app.route("/error/<int:status>")
def error(status):
    return flask.Response(str(status), status=status)
