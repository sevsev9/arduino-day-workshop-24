import express, { Express } from 'express';
import { Server as WebSocketServer, WebSocket } from 'ws';
import { createServer, Server } from 'http';
import { AddressInfo } from 'net';

const app: Express = express();
const server: Server = createServer(app);
const wss: WebSocketServer = new WebSocketServer({ server });

wss.on('connection', (ws) => {
  console.log('A client connected');

  ws.on('message', (data: string) => {
    console.log('Received: %s', data);
    console.log(`Data packet received at ${new Date().toISOString()}`); // Log point for every received data packet

    // Broadcasting the data to all connected clients
    wss.clients.forEach((client) => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(data);
      }
    });
  });
});

app.use(express.static('public'));

server.listen(3000, () => {
  const { port } = server.address() as AddressInfo;
  console.log(`Server started on port ${port}`);
});