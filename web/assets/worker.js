self.importScripts('out.js');
onmessage = (e) => {
  const file = e.data[0];
  if (!FS.analyzePath('/work').exists) {
    FS.mkdir('/work');
  }
  FS.mount(WORKERFS, { files: [file] }, '/work');

  (function () {
    console.log = function () {
      for (var i = 0; i < arguments.length; i++) {
        postMessage(["msg", arguments[0]])
      }
    }
  })();

  const info = Module.transcode('/work/' + file.name);
  if (info == 0) {
    // const files = FS.readdir("/");
    // files.forEach(elm => console.log(elm));
    const file = FS.readFile("/output.mp4")
    const blob = new Blob([file], { type: "video/mp4" });
    postMessage(blob);
  } else {
    postMessage("bad")
  }

  FS.unmount('/work');
}
