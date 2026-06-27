const broker = 'wss://broker.emqx.io:8084/mqtt'; // Broker MQTT
const mqttOptions = {
    username: "Marce", // Usuario MQTT (el mismo que en tu firmware)
    password: "Aveni793", // Contraseña MQTT
};

const clientId = 'mqtt_' + Math.random().toString(16).substr(2, 8); // Genera un ID único para el cliente
const topicPub = 'BALH142N1788/Aveni793'; // Topic de publicación para comandos
const topicSub = 'BALH142N1788/Frente'; // Topic de suscripción para recibir telemetría

const client = mqtt.connect(broker, {
    clientId: clientId,
    ...mqttOptions
});

// Conexión al broker
client.on('connect', () => {
    console.log('Conectado al broker MQTT');
    client.subscribe(topicSub, (err) => {
        if (err) {
            console.error('Error al suscribirse al topic', err);
        } else {
            console.log(`Suscrito a ${topicSub}`);
        }
    });
});

// Recepción de mensajes (telemetría)
client.on('message', (topic, message) => {
    if (topic === topicSub) {
        try {
            const data = JSON.parse(message.toString());

            // Actualiza la interfaz de usuario con los datos recibidos
            document.getElementById('temperatura').textContent = `${data.Temperatura} °C`;
            document.getElementById('humedad').textContent = `${data.Humedad} %`;
            document.getElementById('luz').textContent = data.Luz;
            document.getElementById('movimientos').textContent = data.Movimientos;
            document.getElementById('camaras').textContent = data.Camaras ? 'Encendidas' : 'Apagadas';
            document.getElementById('sirena').textContent = data.Sirena ? 'Encendida' : 'Apagada';
            document.getElementById('reflectores').textContent = data.Reflectores ? 'Encendidos' : 'Apagados';
            document.getElementById('luz-vereda').textContent = data['Luz Vereda'] ? 'Encendida' : 'Apagada';

        } catch (err) {
            console.error('Error al procesar el mensaje recibido:', err);
        }
    }
});

// Función para enviar comandos desde la página
function enviarComando(comando) {
    client.publish(topicPub, JSON.stringify(comando), { qos: 1, retain: false }, (err) => {
        if (err) {
            console.error('Error al enviar comando:', err);
        } else {
            console.log('Comando enviado:', comando);
        }
    });
}

// Control de botones para enviar comandos
document.getElementById('btn-reflectores').addEventListener('click', () => {
    const reflectoresConf = parseInt(document.getElementById('select-reflectores').value);
    enviarComando({ ReflectoresConf: reflectoresConf });
});

document.getElementById('btn-luz-vereda').addEventListener('click', () => {
    const luzVeredaConf = parseInt(document.getElementById('select-luz-vereda').value);
    enviarComando({ LuzVeredaConf: luzVeredaConf });
});

document.getElementById('btn-sirena').addEventListener('click', () => {
    const sirenaConf = parseInt(document.getElementById('select-sirena').value);
    enviarComando({ SirenaConf: sirenaConf });
});

document.getElementById('btn-camaras').addEventListener('click', () => {
    const camarasEn = document.getElementById('select-camaras').checked;
    enviarComando({ CamarasEn: camarasEn });
});

document.getElementById('btn-telemetria').addEventListener('click', () => {
    enviarComando({ Telemetria: true });
});
