const files = [
    {
        name: "notes.txt",
        type: "TXT",
        size: "2.4 MB",
        lastAccess: "94 days ago",
        status: "inactive"
    },

    {
        name: "project.cpp",
        type: "CPP",
        size: "18 KB",
        lastAccess: "103 days ago",
        status: "inactive"
    },

    {
        name: "photo.jpg",
        type: "JPG",
        size: "4.8 MB",
        lastAccess: "120 days ago",
        status: "skipped"
    },

    {
        name: "lecture.mp4",
        type: "MP4",
        size: "840 MB",
        lastAccess: "150 days ago",
        status: "skipped"
    },

    {
        name: "old_report.pdf",
        type: "PDF",
        size: "12 MB",
        lastAccess: "110 days ago",
        status: "archived"
    }
];


const dashboardData = {
    totalFiles: 1284,
    inactiveFiles: 137,
    archivedFiles: 42,
    spaceSaved: 2.84,

    usedStorage: 72,
    freeStorage: 28,

    restoreSafety: 850
};


const fileTable = document.getElementById(
    "fileTable"
);

const filterSelect = document.getElementById(
    "filterSelect"
);


function showStats() {

    document.getElementById(
        "totalFiles"
    ).textContent = dashboardData.totalFiles;

    document.getElementById(
        "inactiveFiles"
    ).textContent = dashboardData.inactiveFiles;

    document.getElementById(
        "archivedFiles"
    ).textContent = dashboardData.archivedFiles;

    document.getElementById(
        "spaceSaved"
    ).textContent =
        dashboardData.spaceSaved + " GB";

    document.getElementById(
        "usedStorage"
    ).textContent =
        dashboardData.usedStorage + " GB";

    document.getElementById(
        "freeStorage"
    ).textContent =
        dashboardData.freeStorage + " GB";

    document.getElementById(
        "restoreSafety"
    ).textContent =
        dashboardData.restoreSafety + " MB";

    const total =
        dashboardData.usedStorage +
        dashboardData.freeStorage;

    const usedPercent =
        (dashboardData.usedStorage / total) * 100;

    document.getElementById(
        "storageBar"
    ).style.width = usedPercent + "%";

    document.getElementById(
        "restoreMessage"
    ).textContent =
        "Current free space: " +
        dashboardData.freeStorage +
        " GB. Some archived files may require up to " +
        dashboardData.restoreSafety +
        " MB to restore.";

}


function statusText(status) {

    if (status === "inactive") {
        return "Inactive";
    }

    if (status === "archived") {
        return "Archived";
    }

    if (status === "skipped") {
        return "Skipped";
    }

    return status;
}


function createRow(file) {

    const row = document.createElement("tr");

    let action = "Review";

    if (file.status === "inactive") {
        action = "Compress";
    }

    if (file.status === "archived") {
        action = "Restore";
    }

    if (file.status === "skipped") {
        action = "Details";
    }

    row.innerHTML = `
        <td>${file.name}</td>
        <td>${file.type}</td>
        <td>${file.size}</td>
        <td>${file.lastAccess}</td>

        <td>
            <span class="status ${file.status}">
                ${statusText(file.status)}
            </span>
        </td>

        <td>
            <button
                class="action-btn"
                onclick="handleAction('${file.name}', '${action}')">
                ${action}
            </button>
        </td>
    `;

    return row;
}


function showFiles(filter = "all") {

    fileTable.innerHTML = "";

    let visibleFiles = files;

    if (filter !== "all") {

        visibleFiles =
            files.filter(
                file => file.status === filter
            );
    }

    visibleFiles.forEach(file => {

        fileTable.appendChild(
            createRow(file)
        );

    });

}


function handleAction(
    fileName,
    action
) {
    alert(
        action +
        " action selected for " +
        fileName
    );
}


function startScan() {

    const status =
        document.getElementById(
            "storageStatus"
        );

    status.textContent = "Scanning...";

    setTimeout(() => {

        status.textContent =
            "Scan complete";

        showStats();
        showFiles();

    }, 700);
}


filterSelect.addEventListener(
    "change",
    function () {
        showFiles(this.value);
    }
);


document.getElementById(
    "scanBtn"
).addEventListener(
    "click",
    startScan
);


document.getElementById(
    "heroScanBtn"
).addEventListener(
    "click",
    startScan
);


showStats();
showFiles();