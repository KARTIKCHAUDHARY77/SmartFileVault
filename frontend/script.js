const API_BASE_URL = "http://localhost:8080";

let usingDemoData = true;

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
 * This object is the only place where the frontend
 * communicates with the C++ backend.
 */

const backend = {

    async getDashboard()
    {
        try {
            const response = await fetch(
                API_BASE_URL + "/api/dashboard"
            );

            if (!response.ok) {
                throw new Error("Dashboard request failed");
            }

            const data = await response.json();

            if (data.success === false) {
                throw new Error("Backend returned an error");
            }

            usingDemoData = false;

            return data;
        }
        catch (error) {
            console.log("Using demo dashboard data.");
            usingDemoData = true;

            return demoDashboard;
        }
    },


    async getArchives()
    {
        try {
            const response = await fetch(
                API_BASE_URL + "/api/archives"
            );

            if (!response.ok) {
                throw new Error("Archive request failed");
            }

            const data = await response.json();

            if (data.success === false) {
                throw new Error("Backend returned an error");
            }

            usingDemoData = false;

            return data;
        }
        catch (error) {
            console.log("Using demo archive data.");
            usingDemoData = true;

            return {
                archiveCount: demoArchives.length,
                originalSize: 0,
                compressedSize: 0,
                spaceSaved: 0,
                restoreSafetySpace: 0,
                freeSpace: 0,
                archives: demoArchives.map(file => ({
                    archiveId: file.id,
                    originalName: file.name,
                    originalPath: file.originalPath,
                    originalSizeText: file.requiredSpace,
                    compressedSizeText: file.size,
                    archivePath: "",
                    compressionPercentage: 0,
                    archivedAt: 0
                }))
            };
        }
    },


    async getFiles()
    {
        /*
         * The scan-report API is not connected yet.
         * Keep the existing demo file list until
         * that backend endpoint is added.
         */
        return demoFiles;
    },


    async scan(folderPath, inactiveDays)
    {
        console.log("Scan request:", {
            folderPath,
            inactiveDays
        });

        /*
         * The current C++ API only exposes read-only
         * dashboard and archive endpoints.
         * Real scan control will be connected later.
         */

        return {
            success: false,
            demo: true,
            message: "Scan API is not connected yet."
        };
    },


    async restore(archiveId, destination)
    {
        console.log("Restore request:", {
            archiveId,
            destination
        });

        /*
         * Restore API will be connected after the
         * backend restore endpoint is added.
         */

        return {
            success: false,
            demo: true
        };
    }
};


function showConnectionStatus()
{
    const badge = document.querySelector(".hero-badge");

    if (!badge) {
        return;
    }

    if (usingDemoData) {

        badge.innerHTML = `
            <strong>Demo Mode</strong>
            <span>C++ API is not connected</span>
        `;
    }
    else {

        badge.innerHTML = `
            <strong>Live Backend</strong>
            <span>Connected to C++ API</span>
        `;
    }
}


function showSection(sectionId)
{
    const sections =
        document.querySelectorAll(".section");

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

    const navItems =
        document.querySelectorAll(".nav-item");

    navItems.forEach(item => {
        item.classList.remove("active");

        if (
            item.textContent.trim().toLowerCase()
            === sectionId
        ) {
            item.classList.add("active");
        }
    });
}


async function updateDashboard()
{
    const data =
        await backend.getDashboard();

    document.getElementById("totalFiles").textContent =
        data.totalFiles ?? data.archiveCount ?? 0;

    document.getElementById("inactiveFiles").textContent =
        data.inactiveFiles ?? "-";

    document.getElementById("archivedFiles").textContent =
        data.archivedFiles ?? data.archiveCount ?? 0;

    document.getElementById("spaceSaved").textContent =
        data.spaceSavedText ??
        data.spaceSaved ??
        "0 B";

    showConnectionStatus();
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

    const fileList =
        await backend.getFiles();

    if (filter === "all") {
        displayFiles(fileList);
        return;
    }

    const filtered =
        fileList.filter(file => {
            return file.status === filter;
        });

    displayFiles(filtered);
}


async function displayRestoreFiles()
{
    const data =
        await backend.getArchives();

    const archives =
        data.archives || [];

    const container =
        document.getElementById("restoreList");

    container.innerHTML = "";

    if (archives.length === 0) {

        container.innerHTML = `
            <p class="note">
                No archived files found.
            </p>
        `;

        return;
    }

    archives.forEach(file => {

        container.innerHTML += `
            <div class="restore-item">

                <div>
                    <strong>${file.originalName}</strong>

                    <span>
                        ${file.originalSizeText || "-"}
                        •
                        ${file.originalPath || "-"}
                    </span>
                </div>

                <button
                    class="action-button"
                    onclick="openRestoreModal('${file.archiveId}')">
                    Restore
                </button>

            </div>
        `;
    });
}


async function openRestoreModal(archiveId)
{
    const data =
        await backend.getArchives();

    const file =
        (data.archives || []).find(item => {
            return item.archiveId === archiveId;
        });

    if (!file) {
        return;
    }

    document.getElementById("modalFileName").textContent =
        file.originalName;

    document.getElementById("modalOriginalPath").textContent =
        file.originalPath || "-";

    document.getElementById("modalRequiredSpace").textContent =
        file.originalSizeText || "-";

    document.getElementById("modalFreeSpace").textContent =
        data.freeSpaceText || "-";

    document.getElementById("restoreDestination").value =
        file.originalPath
            ? file.originalPath.substring(
                0,
                file.originalPath.lastIndexOf("/")
              )
            : "";

    document.getElementById("restoreModal")
        .classList.add("show");

    document.getElementById("restoreModal")
        .dataset.archiveId = archiveId;
}


function closeRestoreModal()
{
    document.getElementById("restoreModal")
        .classList.remove("show");
}


async function confirmRestore()
{
    const archiveId =
        document.getElementById("restoreModal")
            .dataset.archiveId;

    const destination =
        document
            .getElementById("restoreDestination")
            .value
            .trim();

    if (destination === "") {

        alert(
            "Please enter a restore destination."
        );

        return;
    }

    const result =
        await backend.restore(
            archiveId,
            destination
        );

    if (result.success) {

        alert(
            "Restore completed successfully."
        );

        closeRestoreModal();
    }
    else {

        alert(
            "The restore API is not connected yet."
        );
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

    const daysElement =
        document.getElementById("inactiveDays");

    const days =
        daysElement
            ? Number(daysElement.value)
            : 30;

    button.textContent = "Scanning...";
    button.disabled = true;

    const result =
        await backend.scan(
            folderPath,
            days
        );

    button.textContent = "Scan Files";
    button.disabled = false;

    if (result.success) {

        alert(
            "Scan completed successfully."
        );

        await updateDashboard();
        await displayFiles();
        await displayRestoreFiles();

    }
    else {

        alert(
            result.message ||
            "The scan API is not connected yet."
        );
    }
}


function saveSettings()
{
    const days =
        document.getElementById("inactiveDays").value;

    const message =
        document.getElementById("settingsMessage");

    message.textContent =
        "Inactivity period saved: "
        + days
        + " days.";
}


document.addEventListener(
    "DOMContentLoaded",
    async () => {

        await updateDashboard();
        await displayFiles();
        await displayRestoreFiles();

    }
);