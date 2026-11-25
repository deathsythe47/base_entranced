// d_debugForcePowers: force press diagnostics in the server log
#ifndef G_FORCEDEBUG_H
#define G_FORCEDEBUG_H

void FD_ClientReset(int clientNum);
void FD_MarkForcedThink(gentity_t *ent);
void FD_CmdBegin(gentity_t *ent);
void FD_CmdEnd(gentity_t *ent);
void FD_PreGenCmd(gentity_t *ent, const usercmd_t *cmd);
void FD_PostGenCmd(gentity_t *ent);
void FD_ButtonsFrame(gentity_t *ent, const usercmd_t *ucmd);
void FD_PowerStarted(gentity_t *ent, int power);
void FD_PowerStopped(gentity_t *ent, int power, int wasActive);
void FD_EndFrame(gentity_t *ent);
void Cmd_ForceBug_f(gentity_t *ent);
void FD_RunFrame(void);

#endif
