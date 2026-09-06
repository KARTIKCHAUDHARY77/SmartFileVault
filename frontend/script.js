const scanButton = document.getElementById("scanButton");

scanButton.addEventListener("click", function () {

    document.getElementById("filesScanned").textContent = "Scanning...";

    setTimeout(function () {

        document.getElementById("filesScanned").textContent = "0";
        document.getElementById("inactiveFiles").textContent = "0";
        document.getElementById("storageSaved").textContent = "0 MB";

        document.getElementById("fileList").textContent =
            "Backend file scanning will be connected here.";

    }, 500);
});