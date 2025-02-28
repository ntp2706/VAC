function initial() {
  writeToFirstRow("Database", ["No.","Chủ sở hữu","Biển số xe","Ngày hết hạn"])
  adjustColumnWidth("Database",1,50)
  adjustColumnWidth("Database",2,250)
  adjustColumnWidth("Database",3,100)
  adjustColumnWidth("Database",4,100)
  writeToFirstRow("Log", ["Ngày","Giờ","Chủ sở hữu","Biển số xe", "Hình ảnh"])
  adjustColumnWidth("Log",1,150)
  adjustColumnWidth("Log",2,100)
  adjustColumnWidth("Log",3,250)
  adjustColumnWidth("Log",4,100)
  adjustColumnWidth("Log",5,600)
  freezeFirstRow()
}

// xử lý các yêu cầu POST từ esp32 cam và esp8266
// dạng json {
    //"sheet": "/tên bảng tính/",
    //"content1": "/nội dung cột A/", "?" // để truy vết và cập nhật ngày hết hạn, "/" // để xóa hàng
    //"content2": "/nội dung cột B/",
    //"content3": "/nội dung cột C/",
    //"content4": "/nội dung cột D/"
//}

function doPost(e) {
  try {
    if (e.postData && e.postData.contents) {
      const data = JSON.parse(e.postData.contents);

      const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(data.sheet);

      // nếu content1 là "?" hoặc "/"
      if (data.content1 === "?" || data.content1 === "/") {
        const lastRow = sheet.getLastRow();
        const range = sheet.getRange(2, 1, lastRow - 1, 4); // bỏ qua hàng tiêu đề
        const values = range.getValues();

        for (let i = 0; i < values.length; i++) {
          const row = values[i];
          // đối chiếu content2 (cột B) và content3 (cột C) với từng hàng
          if (row[1] === data.content2 && row[2] === data.content3) {
            if (data.content1 === "?") {
              sheet.getRange(i + 2, 4).setValue(data.content4 || "");
              return ContentService.createTextOutput("Updated Successfully.");
            } else if (data.content1 === "/") {
              sheet.deleteRow(i + 2);
              generateOrdinalNumber(); // cập nhật lại số thứ tự
              return ContentService.createTextOutput("Deleted Successfully.");
            }
          }
        }
        return ContentService.createTextOutput("No Matching Content Found.");
      }

      const lastRow = sheet.getLastRow();
      const nextRow = lastRow + 1; // hàng để ghi dữ liệu

      sheet.getRange(nextRow, 1).setValue(data.content1 || ""); // Ghi nội dung cột A
      sheet.getRange(nextRow, 2).setValue(data.content2 || ""); // Ghi nội dung cột B
      sheet.getRange(nextRow, 3).setValue(data.content3 || ""); // Ghi nội dung cột C
      sheet.getRange(nextRow, 4).setValue(data.content4 || ""); // Ghi nội dung cột D

      generateOrdinalNumber(); // cập nhật lại số thứ tự

      return ContentService.createTextOutput("Write Successfully.");
    } else {
      return ContentService.createTextOutput("Write Failed.");
    }
  } catch (error) {
    return ContentService.createTextOutput("Error: " + error.message);
  }
}

// đánh số thứ tự tự động
  function generateOrdinalNumber() {
    const spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = spreadsheet.getSheetByName("Database");
    
    const lastRow = sheet.getLastRow(); // tìm hàng cuối cùng có dữ liệu

    const ordinalNumbers = [];
    for (let i = 1; i <= lastRow - 1; i++) {
      ordinalNumbers.push([i]);
    }
    
    sheet.getRange(2, 1, ordinalNumbers.length, 1).setValues(ordinalNumbers); // ghi số thứ tự vào cột A bắt đầu từ hàng 2
  }

// cập nhật số thứ tự
  function onEdit(e) {
    const editedSheet = e.source.getActiveSheet();
    const sheetName = editedSheet.getName();
    
    if (sheetName == "Database") {
      generateOrdinalNumber();
    }
  }

// ghi tên các trường vào hàng đầu tiên
  function writeToFirstRow(sheetName, data) {
    var spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    var sheet = spreadsheet.getSheetByName(sheetName);
    
    var range = sheet.getRange(1, 1, 1, data.length);

    // xóa data validation, ghi dữ liệu, định dạng căn giữa và in đậm
    range.clearDataValidations();
    range.setValues([data]);
    range.setHorizontalAlignment("center");
    range.setFontWeight("bold");

    Logger.log("Ghi dữ liệu thành công.");
  }

// điều chỉnh độ rộng cột
  function adjustColumnWidth(sheetName, columnIndex, width) {
    var spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    var sheet = spreadsheet.getSheetByName(sheetName);
    
    sheet.setColumnWidth(columnIndex, width);
    
    Logger.log("Kích thước cột đã được thay đổi.");
  }

// ghim hàng
function freezeFirstRow() {
  var spreadsheet = SpreadsheetApp.getActiveSpreadsheet();

  var sheetNames = ["Database", "Log"];

  sheetNames.forEach(function(sheetName) {
    var sheet = spreadsheet.getSheetByName(sheetName);
    if (sheet) {
      sheet.setFrozenRows(1);
      Logger.log("Đã ghim hàng 1 cho bảng tính: " + sheetName);
    }
  });
}


