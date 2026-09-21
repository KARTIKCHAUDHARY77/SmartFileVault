const dashboardData = {
    totalFiles: 1284,
    inactiveFiles: 137,
    archivedFiles: 42,
    spaceSaved: "2.84 GB"
};

const files = [
    {
        name: "notes.txt",
        size: "245 KB",
        activity: "45 days ago",
        status: "inactive"
    },
    {
        name: "project.cpp",
        size: "72 KB",
        activity: "38 days ago",
        status: "inactive"
    },
    {
        name: "college_work.pdf",
        size: "18 MB",
        activity: "82 days ago",
        status: "archived"
    },
    {
        name: "photo.jpg",
        size: "3.2 MB",
        activity: "60 days ago",
        status: "skipped"
    },
    {
        name: "lecture.mp4",
        size: "540 MB",
        activity: "75 days ago",
        status: "skipped"
    },
    {
        name: "current_notes.txt",
        size: "120 KB",
        activity: "2 days ago",
        status: "active"
    }
];

const archivedFiles = [
    {
        name: "college_work.pdf",
        size: "18 MB",
        originalPath: "/Documents/college_work.pdf",
        requiredSpace: "18 MB",
        freeSpace: "28 GB"
    },
    {
        name: "old_project.cpp",
        size: "2.4 MB",
        originalPath: "/Projects/old_project.cpp",
        requiredSpace: "2.4 MB",
        freeSpace: "28 GB"
    }
];


function showSection(sectionId)
{
    const sections = document.querySelectorAll(".section");

    sections.forEach(section => {
        section.classList.remove("active-section");
    });

    document.getElementById(sectionId).classList.add("active-section");

    const titles = {
        dashboard: "Dashboard",
        files: "Files",
        restore: "Restore",
        settings: "Settings"
    };

    document.getElementById("pageTitle").textContent =
        titles[sectionId];

    const navItems = document.querySelectorAll(".nav-item");

    navItems.forEach(item => {
        item.classList.remove("active");
    });

    navItems.forEach(item => {
        if (item.textContent.trim().toLowerCase() === sectionId) {
            item.classList.add("active");
        }
    });
}


function updateDashboard()
{
    document.getElementById("totalFiles").textContent =
        dashboardData.totalFiles;

    document.getElementById("inactiveFiles").textContent =
        dashboardData.inactiveFiles;

    document.getElementById("archivedFiles").textContent =
        dashboardData.archivedFiles;

    document.getElementById("spaceSaved").textContent =
        dashboardData.spaceSaved;
}


function getStatusText(status)
{
    const names = {
        active: "Active",
        inactive: "Inactive",
        archived: "Archived",
        skipped: "Skipped"
    };

    return names[status] || status;
}


function createFileRow(file)
{
    let button = "";

    if (file.status === "archived") {
        button =
            `<button class="action-button"
                onclick="openRestoreModal('${file.name}')">
                Restore
            </button>`;
    }
    else {
        button = `<button class="action-button">View</button>`;
    }

    return `
        <tr>
            <td><strong>${file.name}</strong></td>
            <td>${file.size}</td>
            <td>${file.activity}</td>
            <td>
                <span class="status status-${file.status}">
                    ${getStatusText(file.status)}
                </span>
            </td>
            <td>${button}</td>
        </tr>
    `;
}


function displayFiles(fileList = files)
{
    const table = document.getElementById("fileTable");

    table.innerHTML = "";

    fileList.forEach(file => {
        table.innerHTML += createFileRow(file);
    });
}


function filterFiles()
{
    const filter = document.getElementById("fileFilter").value;

    if (filter === "all") {
        displayFiles(files);
        return;
    }

    const filtered = files.filter(file => {
        return file.status === filter;
    });

    displayFiles(filtered);
}


function displayRestoreFiles()
{
    const container = document.getElementById("restoreList");

    container.innerHTML = "";

    archivedFiles.forEach(file => {

        container.innerHTML += `
            <div class="restore-item">
                <div>
                    <strong>${file.name}</strong>
                    <span>${file.size} • ${file.originalPath}</span>
                </div>

                <button
                    class="action-button"
                    onclick="openRestoreModal('${file.name}')">
                    Restore
                </button>
            </div>
        `;
    });
}


function openRestoreModal(fileName)
{
    const file = archivedFiles.find(item => {
        return item.name === fileName;
    });

    if (!file) {
        return;
    }

    document.getElementById("modalFileName").textContent =
        file.name;

    document.getElementById("modalOriginalPath").textContent =
        file.originalPath;

    document.getElementById("modalRequiredSpace").textContent =
        file.requiredSpace;

    document.getElementById("modalFreeSpace").textContent =
        file.freeSpace;

    document.getElementById("restoreDestination").value = "";

    document.getElementById("restoreModal")
        .classList.add("show");
}


function closeRestoreModal()
{
    document.getElementById("restoreModal")
        .classList.remove("show");
}


function confirmRestore()
{
    const fileName =
        document.getElementById("modalFileName").textContent;

    const destination =
        document.getElementById("restoreDestination").value.trim();

    if (destination === "") {
        alert("Please enter a restore destination.");
        return;
    }

    alert(
        "Restore request prepared for " +
        fileName +
        "\nDestination: " +
        destination +
        "\n\nBackend connection will perform the real restore."
    );

    closeRestoreModal();
}


function startScan()
{
    const button = document.querySelector(".scan-button");

    button.textContent = "Scanning...";
    button.disabled = true;

    setTimeout(() => {

        button.textContent = "Scan Files";
        button.disabled = false;

        alert(
            "Demo scan completed.\n\n" +
            "Real scanning will be connected to the C++ backend."
        );

    }, 1200);
}


function saveSettings()
{
    const days =
        document.getElementById("inactiveDays").value;

    const message =
        document.getElementById("settingsMessage");

    message.textContent =
        "Inactivity period saved: " + days + " days.";
}


document.addEventListener("DOMContentLoaded", () => {

    updateDashboard();
    displayFiles();
    displayRestoreFiles();

});