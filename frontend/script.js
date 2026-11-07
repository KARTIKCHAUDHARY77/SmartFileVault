
const API_BASE_URL = "http://localhost:8080";

let apiConnected = false;

const LAST_SCAN_KEY = "smartFileVaultLastScan";
const INACTIVE_DAYS_KEY = "smartFileVaultInactiveDays";

function readLastScanResult() {
    try {
        const stored = localStorage.getItem(LAST_SCAN_KEY);
        if (!stored) return null;

        const parsed = JSON.parse(stored);
        return parsed && typeof parsed === "object" && !Array.isArray(parsed)
            ? parsed
            : null;
    } catch {
        return null;
    }
}

function readSavedInactiveDays() {
    try {
        const days = Number(localStorage.getItem(INACTIVE_DAYS_KEY));
        return [7, 30, 60, 90].includes(days) ? String(days) : "30";
    } catch {
        return "30";
    }
}

let lastScanResult = readLastScanResult();
let archiveData = [];

function showConnectionStatus(connected) {
    apiConnected = connected;

    const badge = document.querySelector(".hero-badge");

    if (!badge) return;

    if (connected) {
        badge.innerHTML = `
            <strong>Live Backend</strong>
            <span>Connected to C++ API</span>
        `;
    } else {
        badge.innerHTML = `
            <strong>API Unavailable</strong>
            <span>Check whether the C++ server is running</span>
        `;
    }
}

async function getJSON(url, options = {}) {
    try {
        const response = await fetch(url, options);
        const data = await response.json();

        if (!response.ok || data.success === false) {
            throw new Error(data.message || "Request failed.");
        }

        showConnectionStatus(true);
        return data;
    } catch (error) {
        showConnectionStatus(false);
        console.error("API error:", error.message);
        return null;
    }
}

const backend = {
    async getDashboard() {
        return await getJSON(`${API_BASE_URL}/api/dashboard`);
    },

    async getArchives() {
        return await getJSON(`${API_BASE_URL}/api/archives`);
    },

    async scan(folderPath, inactiveDays) {
        try {
            const response = await fetch(`${API_BASE_URL}/api/scan`, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify({
                    folderPath: folderPath,
                    inactiveDays: inactiveDays
                })
            });

            const data = await response.json();

            if (!response.ok || data.success !== true) {
                showConnectionStatus(response.ok);
                return {
                    success: false,
                    message: data.message || "Scan failed."
                };
            }

            showConnectionStatus(true);
            return data;
        } catch (error) {
            showConnectionStatus(false);

            return {
                success: false,
                message: "Cannot connect to the C++ API. Check whether the server is running."
            };
        }
    },

    async deleteArchive(archiveId) {
        try {
            const response = await fetch(
                `${API_BASE_URL}/api/archives/${encodeURIComponent(archiveId)}`,
                { method: "DELETE" }
            );

            const data = await response.json();
            if (!response.ok || data.success !== true) {
                showConnectionStatus(response.ok);
                return {
                    ...data,
                    success: false,
                    message: data.message || "Permanent archive deletion failed."
                };
            }

            showConnectionStatus(true);
            return data;
        } catch (error) {
            showConnectionStatus(false);
            return {
                success: false,
                message: "Cannot connect to the C++ API. Check whether the server is running."
            };
        }
    },

    async restore(archiveId, destination) {
        try {
            const response = await fetch(`${API_BASE_URL}/api/restore`, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify({
                    archiveId: archiveId,
                    destination: destination
                })
            });

            const data = await response.json();

            if (!response.ok || data.success !== true) {
                showConnectionStatus(response.ok);
                return {
                    success: false,
                    message: data.message || "Restore failed."
                };
            }

            showConnectionStatus(true);
            return data;
        } catch (error) {
            showConnectionStatus(false);

            return {
                success: false,
                message: "Cannot connect to the C++ API. Check whether the server is running."
            };
        }
    }
};

function showSection(sectionId) {
    document.querySelectorAll(".section").forEach(section => {
        section.classList.remove("active-section");
    });

    document.querySelectorAll(".nav-item").forEach(item => {
        item.classList.remove("active");
    });

    const section = document.getElementById(sectionId);

    if (section) {
        section.classList.add("active-section");
    }

    const navItem = document.querySelector(
        `.nav-item[onclick*="${sectionId}"]`
    );

    if (navItem) {
        navItem.classList.add("active");
    }

    const titles = {
        dashboard: "Dashboard",
        files: "Files",
        restore: "Restore",
        settings: "Settings"
    };

    const title = document.getElementById("pageTitle");

    if (title) {
        title.textContent = titles[sectionId] || "SmartFileVault";
    }
}


function setText(id, value) {
    const element = document.getElementById(id);

    if (element) {
        element.textContent = value;
    }
}

async function updateDashboard() {
    const data = await backend.getDashboard();

    if (!data) {
        setText("totalFiles", "—");
        setText("inactiveFiles", "—");
        setText("archivedFiles", "—");
        setText("spaceSaved", "—");
        setText("storageFree", "—");
        setText("dashboardRestoreSafety", "—");
        setText("restoreSafetySpace", "—");
        setText("restoreFreeSpace", "—");
        return;
    }

    setText(
        "totalFiles",
        lastScanResult ? (lastScanResult.scannedFiles ?? "—") : "—"
    );

    setText(
        "inactiveFiles",
        lastScanResult ? (lastScanResult.inactiveFiles ?? "—") : "—"
    );

    const visibleArchives = getVisibleArchives(
        Array.isArray(data.archives) ? data.archives : []
    );

    const visibleSpaceSaved = visibleArchives.reduce(
        (total, file) => total + (Number(file.originalSize) - Number(file.compressedSize)),
        0
    );

    setText("archivedFiles", visibleArchives.length);
    setText("spaceSaved", formatBytes(visibleSpaceSaved));
    setText("storageFree", data.freeSpaceText ?? "—");
    setText("storageFreeMain", data.freeSpaceText ?? "—");

    setText(
        "dashboardRestoreSafety",
        data.restoreSafetySpaceText ?? "—"
    );

    setText(
        "restoreSafetySpace",
        data.restoreSafetySpaceText ?? "—"
    );

    setText("restoreFreeSpace", data.freeSpaceText ?? "—");
}

function getStatusText(status) {
    const statuses = {
        active: "Active",
        inactive: "Inactive",
        archived: "Archived",
        restored: "Restored (archive retained)",
        skipped: "Skipped"
    };

    return statuses[status] || status || "Unknown";
}

const RESTORED_ARCHIVES_KEY = "smartFileVaultRestoredArchiveIds";

function getRestoredArchiveIds() {
    try {
        const ids = JSON.parse(
            localStorage.getItem(RESTORED_ARCHIVES_KEY) || "[]"
        );
        return new Set(Array.isArray(ids) ? ids : []);
    } catch {
        return new Set();
    }
}

function markArchiveRestored(archiveId) {
    if (!archiveId) return;

    const ids = getRestoredArchiveIds();
    ids.add(archiveId);

    localStorage.setItem(
        RESTORED_ARCHIVES_KEY,
        JSON.stringify([...ids])
    );
}

function getArchiveRestoreActionText(archiveId) {
    return getRestoredArchiveIds().has(archiveId)
        ? "Restore Again"
        : "Restore";
}

function getVisibleArchives(archives) {
    // Restoring an archive does not remove the compressed archive from the
    // backend vault. Keep it visible so storage totals and delete actions
    // continue to reflect the actual vault contents.
    return archives;
}

function forgetRestoredArchive(archiveId) {
    if (!archiveId) return;

    const ids = getRestoredArchiveIds();
    ids.delete(archiveId);
    localStorage.setItem(RESTORED_ARCHIVES_KEY, JSON.stringify([...ids]));
}
async function displayFiles() {
    const table = document.getElementById("fileTable");
    const statusMessage = document.getElementById("fileStatusMessage");

    if (!table) return;

    table.innerHTML = "";

    const data = await backend.getArchives();

    if (!data) {
        if (statusMessage) {
            statusMessage.textContent = "Could not load file information. Check the API connection.";
        }
        table.innerHTML = `
            <tr>
                <td colspan="5">Unable to load files.</td>
            </tr>
        `;
        return;
    }

    const archives = getVisibleArchives(
        Array.isArray(data.archives) ? data.archives : []
    );
    const filter = document.getElementById("fileFilter")?.value || "all";

    if (statusMessage) {
        statusMessage.textContent = lastScanResult
            ? `Last scan: ${lastScanResult.scannedFiles ?? 0} scanned, ${lastScanResult.inactiveFiles ?? 0} inactive, ${lastScanResult.archivedFiles ?? 0} archived, ${lastScanResult.skippedFiles ?? 0} skipped.`
            : "Showing files currently available in the archive.";
    }

    const files = archives.map(file => ({
        archiveId: file.archiveId,
        name: file.originalName || "Unnamed file",
        path: file.originalPath || "Path unavailable",
        size: file.originalSizeText || "Size unavailable",
        status: getRestoredArchiveIds().has(file.archiveId)
            ? "restored"
            : "archived",
        activity: file.archivedAt
            ? new Date(file.archivedAt * 1000).toLocaleDateString()
            : "Archived"
    }));

    const visibleFiles = filter === "all"
        ? files
        : files.filter(file =>
            filter === "archived" &&
            (file.status === "archived" || file.status === "restored")
        );

    if (visibleFiles.length === 0) {
        const row = document.createElement("tr");
        const cell = document.createElement("td");

        cell.colSpan = 5;
        cell.textContent = filter === "all"
            ? "No archived files yet. Scan results are summarized above. Files will appear here after successful archiving."
            : `No ${filter} files are available from the current API data.`;

        row.appendChild(cell);
        table.appendChild(row);
        return;
    }

    visibleFiles.forEach(file => {
        const row = document.createElement("tr");

        [file.name, file.size, file.activity, getStatusText(file.status)].forEach(value => {
            const cell = document.createElement("td");
            cell.textContent = value;
            row.appendChild(cell);
        });

        const actionCell = document.createElement("td");
        const restoreButton = document.createElement("button");

        restoreButton.type = "button";
        restoreButton.textContent = getArchiveRestoreActionText(file.archiveId);
        restoreButton.addEventListener("click", () => {
            if (file.archiveId) openRestoreModal(file.archiveId);
        });

        const deleteButton = document.createElement("button");
        deleteButton.type = "button";
        deleteButton.textContent = "Delete";
        deleteButton.className = "action-button delete-button";
        deleteButton.addEventListener("click", () => {
            confirmDeleteArchive(file.archiveId);
        });

        actionCell.appendChild(restoreButton);
        actionCell.appendChild(deleteButton);
        row.appendChild(actionCell);
        table.appendChild(row);
    });
}

async function filterFiles() {
    await displayFiles();
}

async function displayRestoreFiles() {
    const list = document.getElementById("restoreList");

    if (!list) return;

    list.innerHTML = "";

    const data = await backend.getArchives();

    if (!data) {
        list.textContent =
            "Could not load archives. Check that the C++ API is running.";
        return;
    }

    archiveData = getVisibleArchives(
        Array.isArray(data.archives) ? data.archives : []
    );

    if (archiveData.length === 0) {
        list.textContent = "No archived files found.";
        return;
    }

    archiveData.forEach(file => {
        const item = document.createElement("div");
        item.className = "restore-item";

        const details = document.createElement("div");
        details.className = "restore-item-details";

        const name = document.createElement("strong");
        name.textContent = file.originalName || "Unnamed file";

        const path = document.createElement("p");
        path.textContent = file.originalPath || "Path unavailable";

        details.appendChild(name);
        details.appendChild(path);

        const button = document.createElement("button");
        button.textContent = getArchiveRestoreActionText(file.archiveId);
        button.type = "button";
        button.addEventListener("click", () => {
            openRestoreModal(file.archiveId);
        });

        const deleteButton = document.createElement("button");
        deleteButton.textContent = "Delete";
        deleteButton.type = "button";
        deleteButton.className = "action-button delete-button";
        deleteButton.addEventListener("click", () => {
            confirmDeleteArchive(file.archiveId);
        });

        item.appendChild(details);
        item.appendChild(button);
        item.appendChild(deleteButton);
        list.appendChild(item);
    });
}

async function confirmDeleteArchive(archiveId) {
    if (!archiveId) {
        alert("No archive selected.");
        return;
    }

    const archivesResponse = await backend.getArchives();
    const archives = Array.isArray(archivesResponse?.archives)
        ? archivesResponse.archives
        : [];
    const archive = archives.find(item => item.archiveId === archiveId);

    if (!archive) {
        alert("Archive not found. Refresh the archive list and try again.");
        await displayFiles();
        await displayRestoreFiles();
        return;
    }

    const confirmationMessage =
        "This will permanently delete the archived file and its original file (if it exists). This action cannot be undone.";

    if (!window.confirm(confirmationMessage)) {
        return;
    }

    const result = await backend.deleteArchive(archiveId);

    // Even a partial failure can have removed the archive artifacts. Refresh
    // the UI whenever the backend confirms the catalogue entry is gone.
    if (result.archiveDeleted) {
        forgetRestoredArchive(archiveId);
        await updateDashboard();
        await displayFiles();
        await displayRestoreFiles();
    }

    if (!result.success) {
        let message = result.message || "Permanent archive deletion failed.";
        if (result.partialFailure) {
            message += "\nSome deletion steps completed. Check the reported status before retrying.";
        }
        alert(message);
        return;
    }

    let message = result.message || "Archive deleted successfully.";
    if (!result.reportUpdated) {
        message += "\nThe vault report could not be refreshed immediately. It will be regenerated when the dashboard or archive list is loaded.";
    }
    alert(message);
}


async function openRestoreModal(archiveId) {
    const file = archiveData.find(
        item => item.archiveId === archiveId
    );

    if (!file) {
        alert("Archive not found. Refresh the archive list and try again.");
        return;
    }

    setText("modalFileName", file.originalName || "Unnamed file");
    setText("modalOriginalPath", file.originalPath || "Path unavailable");
    setText("modalRequiredSpace", file.originalSizeText || "—");

    const data = await backend.getDashboard();

    if (!data) {
        alert("Could not verify available storage. Check the API connection.");
        return;
    }

    setText("modalFreeSpace", data.freeSpaceText ?? "—");

    const path = file.originalPath || "";
    const slash = path.lastIndexOf("/");
    const parentPath = slash > 0 ? path.substring(0, slash) : "";

    const destinationInput = document.getElementById("restoreDestination");

    if (destinationInput) {
        destinationInput.value = parentPath;
    }

    const modal = document.getElementById("restoreModal");

    if (modal) {
        modal.dataset.archiveId = archiveId;
        modal.style.display = "flex";
    }
}

function closeRestoreModal() {
    const modal = document.getElementById("restoreModal");

    if (modal) {
        modal.style.display = "none";
    }
}

async function confirmRestore() {
    const modal = document.getElementById("restoreModal");
    const destinationInput = document.getElementById("restoreDestination");

    const archiveId = modal?.dataset.archiveId;
    const destination = destinationInput?.value.trim();

    if (!archiveId) {
        alert("No archive selected.");
        return;
    }

    if (!destination) {
        alert("Please enter a restore destination.");
        return;
    }

    const result = await backend.restore(archiveId, destination);

    if (!result.success) {
        alert(result.message || "Restore failed.");
        return;
    }
    markArchiveRestored(archiveId);
    alert(
        "File restored successfully.\n" +
        (result.restoredPath || "")
    );

    closeRestoreModal();

    await updateDashboard();
    await displayRestoreFiles();
    await displayFiles();
}
function formatBytes(bytes) {
    if (bytes == null || !Number.isFinite(Number(bytes))) {
        return "—";
    }

    bytes = Number(bytes);

    if (bytes < 1024) {
        return `${bytes} B`;
    }

    if (bytes < 1024 * 1024) {
        return `${(bytes / 1024).toFixed(2)} KB`;
    }

    return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}
async function startScan() {
    const button = document.querySelector(".scan-button");

    const folderPath = prompt(
        "Enter the full path of the folder you want to scan:"
    );

    if (folderPath === null || !folderPath.trim()) {
        return;
    }

    const daysInput = document.getElementById("inactiveDays");
    const inactiveDays = Number(daysInput?.value || 30);

    if (!Number.isInteger(inactiveDays) || inactiveDays <= 0) {
        alert("Please select a valid inactivity period.");
        return;
    }

    const oldText = button ? button.textContent : "";

    if (button) {
        button.disabled = true;
        button.textContent = "Scanning...";
    }

    try {
        const result = await backend.scan(
            folderPath.trim(),
            inactiveDays
        );
        console.log("Scan API response:", result);
        if (!result.success) {
            alert(result.message || "Scan failed.");
            return;
        }

        lastScanResult = result;
        try {
            localStorage.setItem(LAST_SCAN_KEY, JSON.stringify(result));
        } catch (error) {
            console.warn("Could not persist the latest scan summary:", error);
        }
        alert(
            "Scan completed successfully.\n\n" +
            `Files scanned: ${result.scannedFiles ?? "—"}\n` +
            `Inactive files: ${result.inactiveFiles ?? "—"}\n` +
            `Files archived: ${result.archivedFiles ?? "—"}\n` +
            `Files skipped: ${result.skippedFiles ?? "—"}\n` +
            `Space saved: ${formatBytes(result.spaceSaved)}`
        );

        await updateDashboard();
        await displayFiles();
        await displayRestoreFiles();
    } finally {
        if (button) {
            button.disabled = false;
            button.textContent = oldText;
        }
    }
}

function saveSettings() {
    const daysInput = document.getElementById("inactiveDays");
    const message = document.getElementById("settingsMessage");

    const days = Number(daysInput?.value);

    if (!Number.isInteger(days) || days <= 0) {
        if (message) {
            message.textContent = "Please select a valid number of days.";
        }
        return;
    }

    try {
        localStorage.setItem(INACTIVE_DAYS_KEY, String(days));
    } catch (error) {
        if (message) {
            message.textContent = "Could not save this setting in browser storage.";
        }
        return;
    }

    if (message) {
        message.textContent =
            `Inactivity period saved as ${days} days for future scans.`;
    }
}

document.addEventListener("DOMContentLoaded", async () => {
    showConnectionStatus(false);

    const inactiveDaysInput = document.getElementById("inactiveDays");
    if (inactiveDaysInput) {
        inactiveDaysInput.value = readSavedInactiveDays();
    }

    await updateDashboard();
    await displayFiles();
    await displayRestoreFiles();
});
