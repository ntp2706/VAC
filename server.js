const express = require('express');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const { exec } = require('child_process');

const app = express();
const port = 3000;

const cors = require('cors');
app.use(cors()); // Kích hoạt CORS cho tất cả các domain

// Thiết lập thư mục lưu trữ và đổi tên tệp
const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        cb(null, path.join(__dirname, 'uploads'));
    },
    filename: (req, file, cb) => {
        cb(null, 'image.jpg'); // Đổi tên tệp thành image.jpg
    }
});

const upload = multer({ storage: storage });

// Route cho trang chủ
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'src', 'index.html')); // Trả về file index.html
});

let lastResult = null; // Lưu trữ kết quả tạm thời

// Route hiển thị kết quả (trang mới)
app.get('/result', (req, res) => {
    res.sendFile(path.join(__dirname, 'src', 'result.html'));
});

// API để lấy dữ liệu mới nhất từ server
app.get('/api/result', (req, res) => {
    if (!lastResult) {
        return res.status(200).json({ message: 'No result available yet.' });
    }
    res.json(lastResult);
});

// Cập nhật logic trong route upload
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

            // Lưu kết quả để hiển thị trên trang kết quả
            lastResult = {
                plate: plate,
                crop: `data:image/jpeg;base64,${imageData.toString('base64')}`
            };

            // Gửi phản hồi đến client yêu cầu POST
            res.json(lastResult);

            console.log('Recognition result:', plate);
        });
    });
});


// Bắt đầu server
app.listen(port, () => {
    console.log(`Server is running on http://localhost:${port}`);
});
