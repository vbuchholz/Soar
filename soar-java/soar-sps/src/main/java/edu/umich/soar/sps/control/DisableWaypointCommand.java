/**
 * 
 */
package edu.umich.soar.sps.control;

import org.apache.log4j.Logger;

import edu.umich.soar.robot.OffsetPose;

import sml.Agent;
import sml.Identifier;

/**
 * @author voigtjr
 *
 * Disables a waypoint.
 */
final class DisableWaypointCommand extends NoDDCAdapter implements Command {
	private static final Logger logger = Logger.getLogger(DisableWaypointCommand.class);
	static final String NAME = "disable-waypoint";
	
	public boolean execute(InputLinkInterface inputLink, Agent agent,
			Identifier command, OffsetPose opose,
			OutputLinkManager outputLinkManager) {
		String id = command.GetParameterValue("id");
		if (id == null) {
			logger.warn(NAME + ": No id on command");
			CommandStatus.error.addStatus(agent, command);
			return false;
		}

		logger.debug(String.format(NAME + ": %16s", id));

		if (inputLink.disableWaypoint(id) == false) {
			logger.warn(NAME + ": Unable to disable waypoint " + id + ", no such waypoint");
			CommandStatus.error.addStatus(agent, command);
			return false;
		}

		CommandStatus.accepted.addStatus(agent, command);
		CommandStatus.complete.addStatus(agent, command);
		return true;
	}

}
