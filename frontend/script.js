const scanButton = document.getElementById("scanButton");

const filesScanned = document.getElementById("filesScanned");
const inactiveFiles = document.getElementById("inactiveFiles");
const storageSaved = document.getElementById("storageSaved");

const fileTable = document.getElementById("fileTable");
const statusMessage = document.getElementById("statusMessage");

scanButton.addEventListener("click", function () {

    statusMessage.textContent = "Scanning files...";

    // Sample data for the Day 1 frontend prototype
    const files = [
        {
            name: "project_report.pdf",
            size: "2.4 MB",
            status: "Inactive"
        },
        {
            name: "photo.jpg",
            size: "1.8 MB",
            status: "Active"
        },
        {
            name: "notes.txt",
            size: "0.4 MB",
            status: "Inactive"
        }
    ];

    filesScanned.textContent = files.length;

    let inactiveCount = 0;

    fileTable.innerHTML = "";

    files.forEach(function (file) {

        if (file.status === "Inactive") {
            inactiveCount++;
        }

        const row = document.createElement("tr");

        row.innerHTML = `
            <td>${file.name}</td>
            <td>${file.size}</td>
            <td>${file.status}</td>
        `;

        fileTable.appendChild(row);
    });

    inactiveFiles.textContent = inactiveCount;

    storageSaved.textContent = "0 MB";

    statusMessage.textContent =
        "Scan completed. Backend integration will be added later.";
});