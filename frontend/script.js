const demoDashboard = {
    totalFiles: 1284,
    inactiveFiles: 137,
    archivedFiles: 42,
    spaceSaved: "2.84 GB"
};

const demoFiles = [
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

const demoArchives = [
    {
        id: "SFV-DEMO-001",
        name: "college_work.pdf",
        size: "18 MB",
        originalPath: "/Documents/college_work.pdf",
        requiredSpace: "18 MB",
        freeSpace: "28 GB"
    },
    {
        id: "SFV-DEMO-002",
        name: "old_project.cpp",
        size: "2.4 MB",
        originalPath: "/Projects/old_project.cpp",
        requiredSpace: "2.4 MB",
        freeSpace: "28 GB"
    }
];


/*
 * Backend interface
 *
 * These functions are kept separate so that the demo data
 * can later be replaced by the real C++ backend.
 */

const backend = {

    async scan(folderPath, inactiveDays) {

        console.log("Backend scan request:", {
            folderPath,
            inactiveDays
        });

        // Real C++ backend connection will be added here.
        return {
            success: true,
            demo: true,
            message: "Demo scan completed."
        };
    },

    async getDashboard() {

        return demoDashboard;
    },

    async getFiles() {

        return demoFiles;
    },

    async getArchives() {

        return demoArchives;
    },

    async restore(archiveId, destination) {

        console.log("Restore request:", {
            archiveId,
            destination
        });

        // Real C++ restore call will be added here.
        return {
            success: true,
            demo: true
        };
    }
};


function showSection(sectionId)
{
    const sections = document.querySelectorAll(".section");

    sections.forEach(section => {
        section.classList.remove("active-section");
    });

    const selectedSection =
        document.getElementById(sectionId);

    if (selectedSection) {
        selectedSection.classList.add("active-section");
    }

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

        if (item.textContent.trim().toLowerCase() === sectionId) {
            item.classList.add("active");
        }
    });
}


async function updateDashboard()
{
    const data = await backend.getDashboard();

    document.getElementById("totalFiles").textContent =
        data.totalFiles;

    document.getElementById("inactiveFiles").textContent =
        data.inactiveFiles;

    document.getElementById("archivedFiles").textContent =
        data.archivedFiles;

    document.getElementById("spaceSaved").textContent =
        data.spaceSaved;
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
    let action = "";

    if (file.status === "archived") {

        action = `
            <button
                class="action-button"
                onclick="openRestoreModal('${file.name}')">
                Restore
            </button>
        `;
    }
    else {

        action = `
            <button class="action-button">
                View
            </button>
        `;
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

            <td>
                ${action}
            </td>
        </tr>
    `;
}


async function displayFiles(fileList = null)
{
    if (!fileList) {
        fileList = await backend.getFiles();
    }

    const table =
        document.getElementById("fileTable");

    table.innerHTML = "";

    fileList.forEach(file => {
        table.innerHTML += createFileRow(file);
    });
}


async function filterFiles()
{
    const filter =
        document.getElementById("fileFilter").value;

    const files =
        await backend.getFiles();

    if (filter === "all") {
        displayFiles(files);
        return;
    }

    const filtered =
        files.filter(file => file.status === filter);

    displayFiles(filtered);
}


async function displayRestoreFiles()
{
    const archives =
        await backend.getArchives();

    const container =
        document.getElementById("restoreList");

    container.innerHTML = "";

    archives.forEach(file => {

        container.innerHTML += `
            <div class="restore-item">

                <div>
                    <strong>${file.name}</strong>

                    <span>
                        ${file.size} • ${file.originalPath}
                    </span>
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


async function openRestoreModal(fileName)
{
    const archives =
        await backend.getArchives();

    const file =
        archives.find(item => item.name === fileName);

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


async function confirmRestore()
{
    const fileName =
        document.getElementById("modalFileName").textContent;

    const destination =
        document.getElementById("restoreDestination").value.trim();

    if (destination === "") {
        alert("Please enter a restore destination.");
        return;
    }

    const archives =
        await backend.getArchives();

    const file =
        archives.find(item => item.name === fileName);

    if (!file) {
        alert("Archive not found.");
        return;
    }

    const result =
        await backend.restore(file.id, destination);

    if (result.success) {

        alert(
            "Restore request completed for " +
            fileName
        );

        closeRestoreModal();
    }
    else {
        alert("Restore failed.");
    }
}


async function startScan()
{
    const button =
        document.querySelector(".scan-button");

    const folderPath =
        prompt("Enter folder path to scan:");

    if (!folderPath) {
        return;
    }

    const days =
        document.getElementById("inactiveDays")?.value || 30;

    button.textContent = "Scanning...";
    button.disabled = true;

    const result =
        await backend.scan(folderPath, Number(days));

    button.textContent = "Scan Files";
    button.disabled = false;

    if (result.success) {

        alert(
            result.demo
                ? "Demo scan completed. Real backend connection is pending."
                : "Scan completed successfully."
        );

        await updateDashboard();
        await displayFiles();
        await displayRestoreFiles();
    }
    else {
        alert("Scan failed.");
    }
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


document.addEventListener("DOMContentLoaded", async () => {

    await updateDashboard();
    await displayFiles();
    await displayRestoreFiles();

});