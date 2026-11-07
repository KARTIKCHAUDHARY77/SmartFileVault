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
    "Restore Again"
);
assert.equal(
    context.getArchiveRestoreActionText("archive-unrestored"),
    "Restore"
);
console.log("PASS: restored archive offers Restore Again");

assert.equal(
    context.getStatusText("restored"),
    "Restored (archive retained)"
);
console.log("PASS: restored archive status explains that compressed copy remains");

context.forgetRestoredArchive("archive-restored");
assert.equal(
    context.getArchiveRestoreActionText("archive-restored"),
    "Restore"
);
console.log("PASS: deleting an archive clears its local restored marker");

console.log("ALL FRONTEND STATE TESTS PASSED");
