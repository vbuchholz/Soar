/**
 * 
 */
package org.msoar.sps.control;

import java.util.Arrays;

import org.apache.log4j.Logger;

import sml.Agent;
import sml.Identifier;

/**
 * @author voigtjr
 *
 * Add a waypoint to the waypoint system.
 */
final class AddWaypointCommand extends NoDDCAdapter implements Command {
	private static final Logger logger = Logger.getLogger(AddWaypointCommand.class);
	static final String NAME = "add-waypoint";

	public boolean execute(InputLinkInterface inputLink, Agent agent, Identifier command, SplinterState splinter, OutputLinkManager outputLinkManager) {
		String id = command.GetParameterValue("id");
		if (id == null) {
			logger.warn(NAME + ": No id on command");
			CommandStatus.error.addStatus(agent, command);
			return false;
		}

		if (splinter == null) {
			throw new AssertionError();
		}
		
		double[] pos = Arrays.copyOf(splinter.getSplinterPose().pos, splinter.getSplinterPose().pos.length);
		try {
			pos[0] = Double.parseDouble(command.GetParameterValue("x"));
		} catch (NullPointerException ignored) {
			// no x param is ok, use current
		} catch (NumberFormatException e) {
			logger.warn(NAME + ": Unable to parse x: " + command.GetParameterValue("x"));
			CommandStatus.error.addStatus(agent, command);
			return false;
		}

		try {
			pos[1] = Double.parseDouble(command.GetParameterValue("y"));
		} catch (NullPointerException ignored) {
			// no y param is ok, use current
		} catch (NumberFormatException e) {
			logger.warn(NAME + ": Unable to parse y: " + command.GetParameterValue("y"));
			CommandStatus.error.addStatus(agent, command);
			return false;
		}

		logger.debug(String.format(NAME + ": %16s %10.3f %10.3f", id, pos[0], pos[1]));
		inputLink.addWaypoint(pos, id, outputLinkManager.useFloatYawWmes);

		CommandStatus.accepted.addStatus(agent, command);
		CommandStatus.complete.addStatus(agent, command);
		return true;
	}
}
