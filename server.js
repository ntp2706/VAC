const express = require('express');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const { exec } = require('child_process');
const axios = require('axios');

const app = express();
const port = 3000;

const cors = require('cors');
app.use(cors());

const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        cb(null, path.join(__dirname, 'uploads'));
    },
    filename: (req, file, cb) => {
        cb(null, 'image.jpg');
    }
});

const upload = multer({ storage: storage });

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'src', 'index.html'));
});

let lastResult = null;

app.get('/result', (req, res) => {
    res.sendFile(path.join(__dirname, 'src', 'result.html'));
});

app.get('/api/result', (req, res) => {
    if (!lastResult) {
        return res.status(200).json({ message: 'No result available yet.' });
    }
    res.json(lastResult);
});

app.post('/upload', upload.single('image'), (req, res) => {
    if (!req.file) {
        return res.status(400).send('No file uploaded.');
    }

    console.log('File uploaded successfully:', req.file.path);

    const scriptPath = path.join(__dirname, 'src', 'process.py');
    exec(`python "${scriptPath}"`, (error, stdout, stderr) => {
        if (error) {
            console.error('Error running process.py:', error);
            return res.status(500).send('Error running script.');
        }

        let response;
        try {
            response = JSON.parse(stdout);
        } catch (parseError) {
            console.error('Error parsing JSON:', parseError);
            return res.status(500).send('Error parsing script output.');
        }

        const { plate, crop } = response;
        const croppedImageFullPath = path.join(__dirname, crop);

        if (!fs.existsSync(croppedImageFullPath)) {
            return res.status(500).send('Cropped image not found.');
        }

        fs.readFile(croppedImageFullPath, (err, imageData) => {
            if (err) {
                console.error('Error reading cropped image:', err);
                return res.status(500).send('Error reading image.');
            }

            lastResult = {
                plate: plate,
                crop: `data:image/jpeg;base64,${imageData.toString('base64')}`
            };

            const appsScriptUrl = 'https://script.google.com/macros/s/AKfycbxgJ9xWvw_gw_Xs2rKvuJX3RW38xHmxSuuwgLCXQcvBv7gC2p62JX4pUvoA8m7qohsx/exec'; // Thay bằng URL Apps Script của bạn
            const payload = {
                sheet: 'Log',
                content1: '*',
                content2: lastResult.crop,
                content3: '',
                content4: ''
            };

            axios.post(appsScriptUrl, payload)
                .then((response) => {
                    console.log('Data posted to Apps Script successfully:', response.data);
                })
                .catch((error) => {
                    console.error('Error posting data to Apps Script:', error);
                });

            res.json(lastResult);

            console.log('Recognition result:', plate);
        });
    });
});

app.listen(port, () => {
    console.log(`Server is running on http://localhost:${port}`);
});
