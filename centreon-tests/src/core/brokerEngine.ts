import shell from "shelljs"
import sleep from "await-sleep"

export const isBrokerAndEngineConnected = async (): Promise<Boolean> => {
    for(let i = 0; i < 10; ++i) {
        const cbdPort = 5669;

        const ssResultCbd = shell.exec(`ss -plant | grep ${cbdPort}`)
        if(ssResultCbd.code == 0 && ssResultCbd.stdout.includes(cbdPort + "")) {
            return true;
        }

        await sleep(500);
    }

    return false;
}
