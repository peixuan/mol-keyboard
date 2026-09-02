declare module "*.css" {
  const source: string;
  export default source;
}

interface ImportMetaEnv {
  readonly PROD: boolean;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
