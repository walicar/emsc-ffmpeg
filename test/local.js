import express from 'express';
import path from 'path';

const app = express();

var dir = path.join(import.meta.dirname, "..");


app.use(
  "/assets",
  express.static(path.join(dir, "web/assets"), {
    setHeaders: (res, _) => 
      res.setHeader("Cross-Origin-Embedder-Policy", "require-corp")
    ,
  })
);

app.get("/", (_, res) => {
  // Required headers for pthread: https://emscripten.org/docs/porting/pthreads.html
  res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
  res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
  res.setHeader("Cross-Origin-Resource-Policy", "same-site");
  res.sendFile(path.join(dir, "web/index.html"));
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server is running on port http://localhost:${PORT}`);
});