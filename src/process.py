import cv2
from PIL import Image
import configparser
import os
import glob
import asyncio
from winrt.windows.media.ocr import OcrEngine
from winrt.windows.graphics.imaging import BitmapDecoder
from winrt.windows.storage import StorageFile
from winrt.windows.globalization import Language
import json

# Đọc ảnh ngõ vào và chuyển sang ảnh xám
image = cv2.imread('uploads/image.jpg')
image = cv2.resize(image, (352, 288))
gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
#---------------------------------------

# khai báo file xml và config
classifier = cv2.CascadeClassifier('xml/vn.xml')
config = configparser.ConfigParser()
config.read('xml/vn.conf')
#----------------------------

# đọc các thông số từ file config
scaleFactor = float(config['Detection']['scaleFactor'])
minNeighbors = int(config['Detection']['minNeighbors'])
minSize = tuple(map(int, config['Detection']['minSize'].split(',')))

detections = classifier.detectMultiScale(
    gray,
    scaleFactor=scaleFactor,
    minNeighbors=minNeighbors,
    minSize=minSize
)
#----------------------------------

# kiểm tra thư mục lưu trữ và đặt lại
folder = 'crops'
os.makedirs(folder, exist_ok=True)

for file in glob.glob(os.path.join(folder, '*')):
    os.remove(file)
#-------------------------------------

# lọc và chỉ giữ lại ảnh có kích thước lớn nhất
largest_area = 0
largest_crop = None
largest_box = None

for (x, y, w, h) in detections:
    area = w * h
    if area > largest_area:
        largest_area = area
        largest_crop = image[y:y + h, x:x + w]
        largest_box = (x, y, w, h)

if largest_crop is not None:
    x, y, w, h = largest_box

    path = os.path.join(folder, 'crop.jpg')
    cv2.imwrite(path, largest_crop)
#--------------------------------------------

from PIL import Image
import os
import glob

# Đọc ảnh ngõ vào và lấy kích thước
image = Image.open('crops/crop.jpg')
w, h = image.size
#----------------------------------

# kiểm tra thư mục lưu trữ và đặt lại
folder = 'cases'
os.makedirs(folder, exist_ok=True)

for file in glob.glob(os.path.join(folder, '*')):
    os.remove(file)
#-------------------------------------

percentages = [0.5, 0.4, 0.3] # tỉ lệ bị tô đen

# Tô đen nửa dưới
for i, percentage in enumerate(percentages):
    top = Image.new('RGB', (w, h), 'white')
    visible = int(h * (1 - percentage))
    top.paste(image.crop((0, 0, w, visible)), (0, 0))
    top.save(os.path.join(folder, f'case{i + 1}.jpg'))
#-----------------

# Tô đen nửa trên
    bottom = Image.new('RGB', (w, h), 'black')
    visible = int(h * (1 - percentage))
    bottom.paste(image.crop((0, h - visible, w, h)), (0, h - visible))
    bottom.save(os.path.join(folder, f'case{i + 4}.jpg'))
#----------------

async def ocr(image):
    file = await StorageFile.get_file_from_path_async(image)  # lấy ảnh
    stream = await file.open_async(1)  # mở ảnh

    # giải mã
    decoder = await BitmapDecoder.create_async(stream)
    bitmap = await decoder.get_software_bitmap_async()
    #--------

    engine = OcrEngine.try_create_from_language(Language('en'))  # tạo engine

    # nhận diện và lưu kết quả
    result = await engine.recognize_async(bitmap)
    lines = [line.text for line in result.lines]
    return lines
    #-------------------------

async def start():
    results = []
    for i in range(1, 7):
        image = fr'C:\Users\ASUS\alpr\cases\case{i}.jpg'
        lines = await ocr(image)
        results.append(lines)
    return results

results = asyncio.run(start())

# xử lý ban đầu khi nhận diện biển số
def process(results, start, end, valid, replace):
    segment = ''
    max = 0

    # loại bỏ ký tự đặc biệt
    filtered_results = []
    for i in range(start, end):
        if results[i] and results[i][0]:
            filtered = ''.join(c for c in results[i][0] if c in valid)
            if filtered:
                filtered_results.append(filtered)
            else:
                filtered_results.append('')
        else:
            filtered_results.append('')
    #------------------------

    # chọn ra xâu có độ dài dài nhất
    for filtered in filtered_results:
        if len(filtered) > max:
            max = len(filtered)
            segment = filtered
        elif len(filtered) == max and not segment:
            segment = filtered
    #--------------------------------

    if max < 4:
        segment = ''

    if segment:
        for old, new in replace.items():
            segment = segment.replace(old, new) # thay thế các lỗi hay mắc phải

    return segment
#--------------------------------------

valid_top = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789t|!l')
replace_top = {'|': 'I', '!': 'I', 'l': 'I'}
replace_number = {'1': 'I', '8': 'B', 't': 'C', '2': 'Z', '5': 'S', '0': 'D'}

valid_bottom = set('0123456789|!l')
replace_bottom = {'|': '1', '!': '1', 'l': '1', 'I': '1'}
replace_letter = {'I': '1', 'B': '8', 'Z': '2', 'S': '5', 'D': '0'}

# xử lý hàng trên
top = process(results, 0, 3, valid_top, replace_top)

if top:
    # xử lý khi đọc nhầm viền thành ký tự
    if len(top) > 4:
        if top[0] in {'1', 'I'}:
            top = top.replace(top[0], '', 1)
        elif top[-1] in {'1', 'I'}:
            top = top[:-1]
    #--------------------------------------

    # xử lý nếu có bất kỳ số nào bị đọc nhầm thành chữ cái
    top = list(top)
    for i in range(len(top)):
        if i != 2:
            if top[i] in replace_letter:
                top[i] = replace_letter[top[i]]
    #-----------------------------------------------------

    # xử lý nếu ký tự phải là chữ cái bị đọc nhầm thành số
    if top[2] in replace_number:
        top[2] = replace_number[top[2]]
    top = ''.join(top)
    #-----------------------------------------------------
    #---------------

# xử lý hàng dưới
bottom = process(results, 3, 6, valid_bottom, replace_bottom)

if bottom:
    # xử lý khi đọc nhầm viền thành ký tự
    if len(bottom) > 5:
        if bottom[0] in {'1', 'I'}:
            bottom = bottom.replace(bottom[0], '', 1)
        elif bottom[-1] in {'1', 'I'}:
            bottom = bottom[:-1]
    #--------------------------------------

    # xử lý nếu có bất kỳ số nào bị đọc nhầm thành chữ cái
    bottom = list(bottom)
    for i in range(len(bottom)):
        if bottom[i] in replace_letter:
            bottom[i] = replace_letter[bottom[i]]
    bottom = ''.join(bottom)
    #-----------------------------------------------------
    #---------------

plate = f"{top} - {bottom}" # biển số nhận diện được

crop = 'crops/crop.jpg' # biển số được nhận diện và cắt ra

response = {
    "plate": plate,
    "crop": crop
}

print(json.dumps(response))