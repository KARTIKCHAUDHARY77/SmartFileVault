#!/usr/bin/env node
"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");

const storage = new Map();
const documentStub = {
    addEventListener() {
        // Keep DOMContentLoaded dormant; these are pure state tests.
    },
    querySelector() {
        return null;
    },
    getElementById() {
        return null;
    },
    querySelectorAll() {
        return [];
    }
};

const context = {
    console,
    document: documentStub,
    localStorage: {
        getItem(key) {
            return storage.has(key) ? storage.get(key) : null;
        },
        setItem(key, value) {
            storage.set(key, String(value));
        },
        removeItem(key) {
            storage.delete(key);
        }
    },
    fetch: async () => {
        throw new Error("Network access is not part of this frontend state test.");
    },
    alert() {},
    prompt() { return null; },
    window: { confirm() { return false; } },
    Date,
    JSON,
    Number,
    String,
    Set,
    Array,
    Math
};

vm.createContext(context);
const source = fs.readFileSync("frontend/script.js", "utf8");
vm.runInContext(source, context, { filename: "frontend/script.js" });

const archives = [
    { archiveId: "archive-restored", originalName: "restored.txt" },
    { archiveId: "archive-unrestored", originalName: "pending.txt" }
];

context.localStorage.setItem(
    "smartFileVaultRestoredArchiveIds",
    JSON.stringify(["archive-restored"])
);

const visible = context.getVisibleArchives(archives);
assert.equal(visible.length, 2, "restored archive must remain in vault listings");
assert.deepEqual(
    Array.from(visible, item => item.archiveId),
    ["archive-restored", "archive-unrestored"]
);
console.log("PASS: restored archive remains visible in vault views");

assert.equal(
    context.getArchiveRestoreActionText("archive-restored"),
    "Restore"
);
assert.equal(
    context.getArchiveRestoreActionText("archive-unrestored"),
    "Restore"
);
console.log("PASS: restore actions do not depend on stale local markers");

assert.equal(
    context.getStatusText("restored"),
    "Restored"
);
console.log("PASS: restored status no longer claims archive retention");

context.forgetRestoredArchive("archive-restored");
assert.equal(
    context.getArchiveRestoreActionText("archive-restored"),
    "Restore"
);
console.log("PASS: deleting an archive clears its local restored marker");


const sourceForDeleteUI = fs.readFileSync("frontend/script.js", "utf8");
assert.equal((sourceForDeleteUI.match(/deleteButton\.textContent = "Delete Both";/g) || []).length, 2);
assert.match(sourceForDeleteUI, /Permanently delete BOTH the compressed archive and its original file/);
assert.match(sourceForDeleteUI, /file identity recorded when it was archived/);
assert.match(sourceForDeleteUI, /result\.originalPreserved/);
assert.match(sourceForDeleteUI, /result\.originalMissing/);
console.log("PASS: Files and Restore views expose the identity-safe Delete Both action");

const restoreHandlerStart = sourceForDeleteUI.indexOf("async function confirmRestore()");
const restoreHandlerEnd = sourceForDeleteUI.indexOf("function formatBytes()", restoreHandlerStart);
const restoreHandler = sourceForDeleteUI.slice(restoreHandlerStart, restoreHandlerEnd);
assert.doesNotMatch(restoreHandler, /markArchiveRestored/);
assert.match(restoreHandler, /result\.restored !== true/);
assert.match(restoreHandler, /result\.partialFailure/);
console.log("PASS: Restore UI trusts backend state and handles partial success");


console.log("ALL FRONTEND STATE TESTS PASSED");
