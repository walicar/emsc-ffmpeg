self.importScripts('out.js');
onmessage = (e) => {
  const file = e.data[0];
  if (!FS.analyzePath('/work').exists) {
      FS.mkdir('/work');
  }
  FS.mount(WORKERFS, { files: [file] }, '/work');

  const info = Module.Parse('/work/' + file.name);
  console.log(info);

  postMessage(info);

  FS.unmount('/work');
}
