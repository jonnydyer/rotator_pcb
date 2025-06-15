Import("env")
def before_uploadfs(source, target, env):
    env.Execute("pio run -t uploadfs")
env.AddPreAction("upload", before_uploadfs) 