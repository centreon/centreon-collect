import shell from "shelljs"

export const isBorkerAndEngineConnected = (): Boolean => {
    const lsOfResult = shell.exec('lsof | grep cbmod')
    expect(lsOfResult.stdout).toContain('')
    expect(lsOfResult.code).toBe(0)

    if(lsOfResult.code !== 0 || !lsOfResult.stdout.includes("/usr/lib64/nagios/chmod.so")) {
        throw new Error("lsof | grep cbmod did not contain any entry to chmod")
    }

    // TODO: change to correct check when connection fixed
    const ssResultCbd = shell.exec('ss | grep cbd')
    if(ssResultCbd.code !== 0 || !ssResultCbd.stdout.includes("/usr/lib64/nagios/chmod.so")) {
        throw new Error("ss | grep cbd did not contain any entry to cbd")
    }

        // TODO: change to correct check when connection fixed
    const ssResultCentEngine = shell.exec('ss | grep cbmod')
    if(ssResultCentEngine.code !== 0 || !ssResultCentEngine.stdout.includes("/usr/lib64/nagios/chmod.so")) {
        throw new Error("ss | grep cbd did not contain any entry to cbd")
    }

    return true;
}