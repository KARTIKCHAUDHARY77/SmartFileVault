const files = [
    {
        name: "notes.txt",
        type: "TXT",
        size: "2.4 MB",
        lastActivity: "94 days ago",
        status: "inactive",
        reason: "Candidate for compression"
    },

    {
        name: "project.cpp",
        type: "CPP",
        size: "18 KB",
        lastActivity: "103 days ago",
        status: "inactive",
        reason: "Candidate for compression"
    },

    {
        name: "college_work.pdf",
        type: "PDF",
        size: "12 MB",
        lastActivity: "110 days ago",
        status: "archived",
        reason: "Compressed and stored",
        requiredSpace: "12 MB"
    },

    {
        name: "photo.jpg",
        type: "JPG",
        size: "4.8 MB",
        lastActivity: "120 days ago",
        status: "skipped",
        reason: "Already compressed format"
    },

    {
        name: "lecture.mp4",
        type: "MP4",
        size: "840 MB",
        lastActivity: "150 days ago",
        status: "skipped",
        reason: "Already compressed media"
    },

    {
        name: "current_notes.txt",
        type: "TXT",
        size: "1.1 MB",
        lastActivity: "2 days ago",
        status: "active",
        reason: "Recently modified"
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


const fileTable =
    document.getElementById("fileTable");

const filterSelect =
    document.getElementById("filterSelect");

const inactivityDays =
    document.getElementById("inactivityDays");


function showStats() {

    document.getElementById("totalFiles")
        .textContent =
        dashboardData.totalFiles;

    document.getElementById("inactiveFiles")
        .textContent =
        dashboardData.inactiveFiles;

    document.getElementById("archivedFiles")
        .textContent =
        dashboardData.archivedFiles;

    document.getElementById("spaceSaved")
        .textContent =
        dashboardData.spaceSaved + " GB";


    document.getElementById("usedStorage")
        .textContent =
        dashboardData.usedStorage + " GB";

    document.getElementById("freeStorage")
        .textContent =
        dashboardData.freeStorage + " GB";

    document.getElementById("restoreSafety")
        .textContent =
        dashboardData.restoreSafety + " MB";

    document.getElementById("restoreFreeSpace")
        .textContent =
        dashboardData.freeStorage + " GB";

    document.getElementById("restoreRequirement")
        .textContent =
        dashboardData.restoreSafety + " MB";


    const totalStorage =
        dashboardData.usedStorage +
        dashboardData.freeStorage;

    const usedPercent =
        (dashboardData.usedStorage /
            totalStorage) * 100;

    document.getElementById("storageBar")
        .style.width =
        usedPercent + "%";


    const freeMB =
        dashboardData.freeStorage * 1024;

    const warning =
        document.getElementById(
            "restoreWarning"
        );


    if (
        freeMB <
        dashboardData.restoreSafety
    ) {

        warning.textContent =
            "Warning: some archived files may " +
            "not have enough space to restore.";

        warning.classList.add(
            "warning"
        );

    }
    else {

        warning.textContent =
            "Current storage is sufficient " +
            "for the current archive set.";

        warning.classList.remove(
            "warning"
        );
    }
}


function createStatus(status) {

    const span =
        document.createElement("span");

    span.className =
        "status " + status;


    if (status === "active") {
        span.textContent = "Active";
    }
    else if (status === "inactive") {
        span.textContent = "Inactive";
    }
    else if (status === "archived") {
        span.textContent = "Archived";
    }
    else {
        span.textContent = "Skipped";
    }


    return span;
}


function getAction(status) {

    if (status === "inactive") {
        return "Review";
    }

    if (status === "archived") {
        return "Restore";
    }

    if (status === "skipped") {
        return "Details";
    }

    return "View";
}


function createRow(file) {

    const row =
        document.createElement("tr");


    row.innerHTML = `
        <td>${file.name}</td>
        <td>${file.type}</td>
        <td>${file.size}</td>
        <td>${file.lastActivity}</td>
    `;


    const statusCell =
        document.createElement("td");

    statusCell.appendChild(
        createStatus(file.status)
    );

    row.appendChild(statusCell);


    const reasonCell =
        document.createElement("td");

    reasonCell.textContent =
        file.reason;

    row.appendChild(reasonCell);


    const actionCell =
        document.createElement("td");

    const button =
        document.createElement("button");

    button.className =
        "action-btn";

    button.textContent =
        getAction(file.status);


    button.addEventListener(
        "click",
        function () {

            if (
                file.status === "archived"
            ) {

                openRestoreModal(file);

            }
            else {

                showFileMessage(file);

            }

        }
    );


    actionCell.appendChild(button);

    row.appendChild(actionCell);


    return row;
}


function showFiles(filter = "all") {

    fileTable.innerHTML = "";

    let visibleFiles = files;


    if (filter !== "all") {

        visibleFiles =
            files.filter(
                file =>
                    file.status === filter
            );

    }


    visibleFiles.forEach(
        file => {

            fileTable.appendChild(
                createRow(file)
            );

        }
    );
}


function showFileMessage(file) {

    alert(
        file.name +
        "\n\nStatus: " +
        file.status +
        "\nReason: " +
        file.reason
    );
}


function openRestoreModal(file) {

    const modal =
        document.getElementById(
            "restoreModal"
        );

    const fileName =
        document.getElementById(
            "restoreFileName"
        );

    const originalPath =
        document.getElementById(
            "restoreOriginalPath"
        );

    const requiredSpace =
        document.getElementById(
            "restoreRequiredSpace"
        );

    const availableSpace =
        document.getElementById(
            "restoreAvailableSpace"
        );

    const description =
        document.getElementById(
            "restoreDescription"
        );

    const warning =
        document.getElementById(
            "restoreWarningModal"
        );

    const location =
        document.getElementById(
            "restoreLocation"
        );


    fileName.textContent =
        "Restore " + file.name;


    originalPath.textContent =
        "/Users/user/Documents/" +
        file.name;


    requiredSpace.textContent =
        file.requiredSpace || "150 MB";


    availableSpace.textContent =
        dashboardData.freeStorage +
        " GB";


    description.textContent =
        "SmartFileVault will try to restore " +
        "the file to its original location.";


    warning.classList.add(
        "hidden"
    );


    location.value =
        "/Users/user/Documents";


    modal.dataset.fileName =
        file.name;


    modal.classList.remove(
        "hidden"
    );
}


function closeRestoreModal() {

    document.getElementById(
        "restoreModal"
    ).classList.add(
        "hidden"
    );
}


function confirmRestore() {

    const modal =
        document.getElementById(
            "restoreModal"
        );

    const fileName =
        modal.dataset.fileName;

    const location =
        document.getElementById(
            "restoreLocation"
        ).value.trim();

    const warning =
        document.getElementById(
            "restoreWarningModal"
        );


    if (location === "") {

        warning.textContent =
            "Please enter a restoration location.";

        warning.classList.remove(
            "hidden"
        );

        return;
    }


    /*
     * Frontend prototype only.
     * The real C++ restoration function
     * will be connected later.
     */
    warning.textContent =
        fileName +
        " will be restored to:\n" +
        location;

    warning.classList.remove(
        "hidden"
    );
}


function startScan() {

    const storageStatus =
        document.getElementById(
            "storageStatus"
        );

    const scanStatus =
        document.getElementById(
            "scanStatus"
        );

    const selectedDays =
        inactivityDays.value;


    storageStatus.textContent =
        "Scanning...";


    scanStatus.textContent =
        "Scanning using " +
        selectedDays +
        "-day threshold...";


    setTimeout(
        function () {

            storageStatus.textContent =
                "Scan complete";

            scanStatus.textContent =
                "Scan complete · " +
                selectedDays +
                "-day threshold";

            showStats();

            showFiles();

        },
        800
    );
}


filterSelect.addEventListener(
    "change",
    function () {

        showFiles(
            this.value
        );

    }
);


inactivityDays.addEventListener(
    "change",
    function () {

        const scanStatus =
            document.getElementById(
                "scanStatus"
            );

        scanStatus.textContent =
            "Threshold changed to " +
            this.value +
            " days";
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


document.getElementById(
    "closeRestore"
).addEventListener(
    "click",
    closeRestoreModal
);


document.getElementById(
    "cancelRestore"
).addEventListener(
    "click",
    closeRestoreModal
);


document.getElementById(
    "confirmRestore"
).addEventListener(
    "click",
    confirmRestore
);


showStats();

showFiles();