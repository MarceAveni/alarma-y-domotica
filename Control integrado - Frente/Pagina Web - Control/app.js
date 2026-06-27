// Conectar al broker MQTT
const client = mqtt.connect('mqtt://mqtt.eclipseprojects.io:1883/'); // Cambia el broker si es necesario

// Al conectarse exitosamente
client.on('connect', function () {
  console.log('Conectado al broker MQTT');
  client.subscribe('BALH142N1788/Frente');
});

// Al recibir un mensaje del ESP8266
client.on('message', function (topic, message) {
  const telemetry = JSON.parse(message.toString());
  document.getElementById('temp').innerText = `Temperatura: ${telemetry.Temperatura}°C`;
  document.getElementById('hum').innerText = `Humedad: ${telemetry.Humedad}%`;
  document.getElementById('mov').innerText = `Movimientos detectados: ${telemetry.Movimientos}`;
});

// Publicar comandos MQTT
function publishReflectores(state) {
  const message = {
    ReflectoresConf: state
  };
  client.publish('BALH142N1788/Aveni793', JSON.stringify(message));
}

function publishSirena(state) {
  const message = {
    SirenaConf: state
  };
  client.publish('BALH142N1788/Aveni793', JSON.stringify(message));
}