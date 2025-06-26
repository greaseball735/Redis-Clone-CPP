const WebSocket = require('ws');
const net = require('net');

class DatabaseBridge {
    constructor(dbHost = '127.0.0.1', dbPort = 1234, wsPort = 8080) {
        this.dbHost = dbHost;
        this.dbPort = dbPort;
        this.wsPort = wsPort;
        this.wss = new WebSocket.Server({ port: wsPort });
        
        console.log(`WebSocket server starting on port ${wsPort}`);
        console.log(`Will connect to database at ${dbHost}:${dbPort}`);
        
        this.wss.on('connection', this.handleWebSocketConnection.bind(this));
    }

    handleWebSocketConnection(ws) {
        console.log('New WebSocket connection established');
        
        // Create TCP connection to your custom database
        const dbSocket = new net.Socket();
        
        dbSocket.connect(this.dbPort, this.dbHost, () => {
            console.log('Connected to custom database');
            ws.send(JSON.stringify({ type: 'connected', message: 'Connected to database' }));
        });

        // Handle messages from web client
        ws.on('message', (message) => {
            try {
                const data = JSON.parse(message);
                if (data.type === 'command' && data.cmd) {
                    this.sendCommand(dbSocket, data.cmd, (response) => {
                        // Transform the response to match frontend expectations
                        const transformedResponse = this.transformResponseForFrontend(response);
                        ws.send(JSON.stringify({ 
                            type: 'response', 
                            id: data.id,
                            data: transformedResponse
                        }));
                    });
                }
            } catch (error) {
                console.error('Error parsing WebSocket message:', error);
                ws.send(JSON.stringify({ 
                    type: 'error', 
                    message: 'Invalid message format' 
                }));
            }
        });

        // Handle database connection errors
        dbSocket.on('error', (error) => {
            console.error('Database connection error:', error);
            ws.send(JSON.stringify({ 
                type: 'error', 
                message: 'Database connection error' 
            }));
        });

        // Clean up on WebSocket close
        ws.on('close', () => {
            console.log('WebSocket connection closed');
            dbSocket.end();
        });

        // Clean up on database disconnect
        dbSocket.on('close', () => {
            console.log('Database connection closed');
            if (ws.readyState === WebSocket.OPEN) {
                ws.close();
            }
        });
    }

    transformResponseForFrontend(parsedResponse) {
        // Handle error responses
        if (parsedResponse.error) {
            return { error: parsedResponse.error };
        }
        
        // Handle error type responses from database
        if (parsedResponse.type === 'error') {
            return { error: parsedResponse.message || 'Database error' };
        }
        
        // Handle nil responses
        if (parsedResponse.type === 'nil') {
            return { value: null };
        }
        
        // For all other types, return the value directly
        // This handles string, int, double, and array types
        console.log(this.parseResponse.value);
        return { value: parsedResponse.value };
    }

    sendCommand(socket, cmd, callback) {
        try {
            // Build the request packet like your C++ client does
            let len = 4; // for cmd array length
            for (const s of cmd) {
                len += 4 + Buffer.byteLength(s, 'utf8');
            }

            if (len > 4096) {
                callback({ error: 'Command too long' });
                return;
            }

            const wbuf = Buffer.alloc(4 + len);
            let pos = 0;

            // Write total length
            wbuf.writeUInt32LE(len, pos);
            pos += 4;

            // Write number of arguments
            wbuf.writeUInt32LE(cmd.length, pos);
            pos += 4;

            // Write each argument
            for (const s of cmd) {
                const strBuf = Buffer.from(s, 'utf8');
                wbuf.writeUInt32LE(strBuf.length, pos);
                pos += 4;
                strBuf.copy(wbuf, pos);
                pos += strBuf.length;
            }

            // Send the request
            socket.write(wbuf);

            // Read the response
            this.readResponse(socket, callback);
        } catch (error) {
            console.error('Error sending command:', error);
            callback({ error: error.message });
        }
    }

    readResponse(socket, callback) {
        // Read 4-byte header first
        let headerBuf = Buffer.alloc(4);
        let headerReceived = 0;

        const onData = (data) => {
            if (headerReceived < 4) {
                // Still reading header
                const needed = 4 - headerReceived;
                const available = Math.min(needed, data.length);
                data.copy(headerBuf, headerReceived, 0, available);
                headerReceived += available;
                
                if (headerReceived < 4) {
                    return; // Need more header data
                }
                
                // Header complete, now read body
                const bodyLen = headerBuf.readUInt32LE(0);
                let bodyBuf = Buffer.alloc(bodyLen);
                let bodyReceived = 0;
                
                // Process remaining data from this packet
                if (data.length > available) {
                    const remainingData = data.subarray(available);
                    const bodyAvailable = Math.min(bodyLen, remainingData.length);
                    remainingData.copy(bodyBuf, 0, 0, bodyAvailable);
                    bodyReceived += bodyAvailable;
                }

                if (bodyReceived >= bodyLen) {
                    socket.removeListener('data', onData);
                    callback(this.parseResponse(bodyBuf));
                    return;
                }

                // Set up body reading
                const onBodyData = (bodyData) => {
                    const needed = bodyLen - bodyReceived;
                    const available = Math.min(needed, bodyData.length);
                    bodyData.copy(bodyBuf, bodyReceived, 0, available);
                    bodyReceived += available;

                    if (bodyReceived >= bodyLen) {
                        socket.removeListener('data', onBodyData);
                        callback(this.parseResponse(bodyBuf));
                    }
                };
                socket.on('data', onBodyData);
            }
        };

        socket.on('data', onData);
    }

    parseResponse(buf) {
        if (buf.length < 1) {
            return { error: 'Bad response' };
        }

        const tag = buf[0];
        switch (tag) {
            case 0: // TAG_NIL
                return { type: 'nil', value: null };
                
            case 1: // TAG_ERR
                if (buf.length < 9) return { error: 'Bad response' };
                const code = buf.readInt32LE(1);
                const msgLen = buf.readUInt32LE(5);
                if (buf.length < 9 + msgLen) return { error: 'Bad response' };
                const message = buf.subarray(9, 9 + msgLen).toString('utf8');
                return { type: 'error', code, message };
                
            case 2: // TAG_STR
                if (buf.length < 5) return { error: 'Bad response' };
                const strLen = buf.readUInt32LE(1);
                if (buf.length < 5 + strLen) return { error: 'Bad response' };
                const str = buf.subarray(5, 5 + strLen).toString('utf8');
                return { type: 'string', value: str };
                
            case 3: // TAG_INT
                if (buf.length < 9) return { error: 'Bad response' };
                const intVal = buf.readBigInt64LE(1);
                return { type: 'int', value: Number(intVal) };
                
            case 4: // TAG_DBL
                if (buf.length < 9) return { error: 'Bad response' };
                const dblVal = buf.readDoubleLE(1);
                return { type: 'double', value: dblVal };
                
            case 5: // TAG_ARR
                if (buf.length < 5) return { error: 'Bad response' };
                const arrLen = buf.readUInt32LE(1);
                const arr = [];
                let pos = 5;
                
                for (let i = 0; i < arrLen; i++) {
                    const item = this.parseResponse(buf.subarray(pos));
                    if (item.error) return item;
                    arr.push(item);
                    pos += this.getResponseSize(buf.subarray(pos));
                }
                
                return { type: 'array', value: arr };
                
            default:
                return { error: 'Unknown response type' };
        }
    }

    getResponseSize(buf) {
        if (buf.length < 1) return 0;
        
        const tag = buf[0];
        switch (tag) {
            case 0: return 1;
            case 1: 
                if (buf.length < 9) return 0;
                return 9 + buf.readUInt32LE(5);
            case 2:
                if (buf.length < 5) return 0;
                return 5 + buf.readUInt32LE(1);
            case 3: return 9;
            case 4: return 9;
            case 5:
                if (buf.length < 5) return 0;
                const arrLen = buf.readUInt32LE(1);
                let size = 5;
                for (let i = 0; i < arrLen; i++) {
                    const itemSize = this.getResponseSize(buf.subarray(size));
                    if (itemSize === 0) return 0;
                    size += itemSize;
                }
                return size;
            default: return 0;
        }
    }
}

// Start the bridge server
const bridge = new DatabaseBridge();
console.log('Database bridge server is running...');