import shell from "shelljs"
import sleep from "await-sleep"

export const isBorkerAndEngineConnected = async (): Promise<Boolean> => {

    // TODO: change to correct check when lsof fixed

    // const lsOfResult = shell.exec('lsof | grep cbmod')
    // if(lsOfResult.code !== 0 || !lsOfResult.stdout.includes("/usr/lib64/nagios/chmod.so")) {
    //     throw new Error("lsof | grep cbmod did not contain any entry to chmod")
    // }


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