import shell from "shelljs";
import sleep from "await-sleep";
import { Engine } from "./engine";

export const isBrokerAndEngineConnected = async (
  port: number = 5669
): Promise<boolean> => {
  const limit = Date.now() + 20000;
  while (Date.now() < limit) {
    const ssResultCbd = shell.exec(`ss -plant | grep ${port}`);
    let lines: string[] = ssResultCbd.stdout.split("\n");
    lines = lines.filter(
      (v) => v.includes(port + "") && v.includes("cbd") && v.includes("ESTAB")
    );
    if (lines.length == Engine.instanceCount) return true;

    await sleep(200);
  }
  return false;
};
