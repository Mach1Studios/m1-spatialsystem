// app.js
require('dotenv').config();
const express = require('express');
const axios = require('axios');
const app = express();

// Middleware to parse JSON bodies
app.use(express.json());

const MIXPANEL_TOKEN = process.env.MIXPANEL_PROJECT_TOKEN;
const MIXPANEL_ENDPOINT = 'https://api.mixpanel.com/track/';

if (!MIXPANEL_TOKEN) {
  console.error('Mixpanel project token is not set in the .env file.');
  process.exit(1);
}

// Endpoint to receive tracking data from the JUCE plugin
app.post('/track', async (req, res) => {
  try {
    const clientData = req.body;

    // Validate the received data
    if (!clientData || typeof clientData !== 'object') {
      return res.status(400).json({ error: 'Invalid data format' });
    }

    // Add the Mixpanel project token
    if (!clientData.properties) {
      clientData.properties = {};
    }
    clientData.properties.token = MIXPANEL_TOKEN;

    // Encode the event data as Base64
    const eventData = Buffer.from(JSON.stringify(clientData)).toString('base64');

    // Send the data to Mixpanel
    const response = await axios.post(MIXPANEL_ENDPOINT, null, {
      params: {
        data: eventData,
        verbose: 1,
      },
    });

    // Forward Mixpanel's response back to the client
    res.status(response.status).json(response.data);
  } catch (error) {
    console.error('Error forwarding data to Mixpanel:', error.message);
    res.status(500).json({ error: 'Failed to forward data to Mixpanel' });
  }
});

// Start the server
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Proxy server is running on port ${PORT}`);
});
