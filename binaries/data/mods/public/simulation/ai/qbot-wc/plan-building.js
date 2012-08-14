var BuildingConstructionPlan = function(gameState, type, position) {
	this.type = gameState.applyCiv(type);
	this.position = position;

	this.template = gameState.getTemplate(this.type);
	if (!this.template) {
		this.invalidTemplate = true;
		this.template = undefined;
		debug("Cannot build " + this.type);
		return;
	}
	this.category = "building";
	this.cost = new Resources(this.template.cost());
	this.number = 1; // The number of buildings to build
};

BuildingConstructionPlan.prototype.canExecute = function(gameState) {
	if (this.invalidTemplate){
		return false;
	}

	// TODO: verify numeric limits etc

	var builders = gameState.findBuilders(this.type);

	return (builders.length != 0);
};

BuildingConstructionPlan.prototype.execute = function(gameState) {

	var builders = gameState.findBuilders(this.type).toEntityArray();

	// We don't care which builder we assign, since they won't actually
	// do the building themselves - all we care about is that there is
	// some unit that can start the foundation

	var pos = this.findGoodPosition(gameState);
	if (!pos){
		debug("No room to place " + this.type);
		return;
	}

	builders[0].construct(this.type, pos.x, pos.z, pos.angle);
};

BuildingConstructionPlan.prototype.getCost = function() {
	return this.cost;
};

BuildingConstructionPlan.prototype.findGoodPosition = function(gameState) {
	var template = gameState.getTemplate(this.type);

	var cellSize = gameState.cellSize; // size of each tile

	// First, find all tiles that are far enough away from obstructions:

	var obstructionMap = Map.createObstructionMap(gameState,template);
	
	///obstructionMap.dumpIm("obstructions.png");

	obstructionMap.expandInfluences();
	
	// Compute each tile's closeness to friendly structures:

	var friendlyTiles = new Map(gameState);
	
	var alreadyHasHouses = false;
	
	// If a position was specified then place the building as close to it as possible
	if (this.position){
		var x = Math.round(this.position[0] / cellSize);
		var z = Math.round(this.position[1] / cellSize);
		friendlyTiles.addInfluence(x, z, 200);
	}else{
		// No position was specified so try and find a sensible place to build
		gameState.getOwnEntities().forEach(function(ent) {
			if (ent.hasClass("Structure")) {
				var infl = 32;
				if (ent.hasClass("CivCentre"))
					infl *= 4;
	
				var pos = ent.position();
				var x = Math.round(pos[0] / cellSize);
				var z = Math.round(pos[1] / cellSize);
										   
				if (ent.buildCategory() == "Wall") {	// no real blockers, but can't build where they are
					friendlyTiles.addInfluence(x, z, 2,-1000);
					return;
				}

				if (template._template.BuildRestrictions.Category === "Field"){
					if (ent.resourceDropsiteTypes() && ent.resourceDropsiteTypes().indexOf("food") !== -1){
						if (ent.hasClass("CivCentre"))
							friendlyTiles.addInfluence(x, z, infl/4, infl);
						else
							 friendlyTiles.addInfluence(x, z, infl, infl);
										   
					}
				}else{
					if (template.genericName() == "House" && ent.genericName() == "House") {
						friendlyTiles.addInfluence(x, z, 15.0,20,'linear');	// houses are close to other houses
						alreadyHasHouses = true;
					} else if (template.genericName() == "House") {
						friendlyTiles.addInfluence(x, z, Math.ceil(infl/2.0),infl);	// houses are farther away from other buildings but houses
						friendlyTiles.addInfluence(x, z, Math.ceil(infl/4.0),-infl/2.0);	// houses are farther away from other buildings but houses
					} else if (ent.genericName() != "House") // houses have no influence on other buildings
						friendlyTiles.addInfluence(x, z, infl);
						// If this is not a field add a negative influence near the CivCentre because we want to leave this
						// area for fields.
					if (ent.hasClass("CivCentre") && template.genericName() != "House"){
						friendlyTiles.addInfluence(x, z, Math.floor(infl/8), Math.floor(-infl/2));
					} else if (ent.hasClass("CivCentre")) {
						friendlyTiles.addInfluence(x, z, infl/3.0, infl + 1);
						friendlyTiles.addInfluence(x, z, Math.ceil(infl/5.0), -(infl/2.0), 'linear');
					}
				}
			}
		});
	}
	
	//friendlyTiles.dumpIm("Building " +gameState.getTimeElapsed() + ".png",	200);

	
	// Find target building's approximate obstruction radius, and expand by a bit to make sure we're not too close, this
	// allows room for units to walk between buildings.
	// note: not for houses and dropsites who ought to be closer to either each other or a resource.
	// also not for fields who can be stacked quite a bit
	if (template.genericName() == "Field")
		var radius = Math.ceil(template.obstructionRadius() / cellSize) - 0.7;
	else if (template.buildCategory() === "Dock")
		var radius = 0;
	else if (template.genericName() != "House" && !template.hasClass("DropsiteWood") && !template.hasClass("DropsiteStone") && !template.hasClass("DropsiteMetal"))
		var radius = Math.ceil(template.obstructionRadius() / cellSize) + 1;
	else
		var radius = Math.ceil(template.obstructionRadius() / cellSize);
	
	// further contract cause walls
	if (gameState.playerData.civ == "iber")
		radius *= 0.95;

	// Find the best non-obstructed
	if (template.genericName() == "House" && !alreadyHasHouses) {
		// try to get some space first
		var bestTile = friendlyTiles.findBestTile(10, obstructionMap);
		var bestIdx = bestTile[0];
		var bestVal = bestTile[1];
	} else if (template.genericName() == "House") {
		radius *= 0.9;
	}
	if (bestVal === undefined || bestVal === -1) {
		var bestTile = friendlyTiles.findBestTile(radius, obstructionMap);
		var bestIdx = bestTile[0];
		var bestVal = bestTile[1];
	}
	if (bestVal === -1){
		return false;
	}
	
	var x = ((bestIdx % friendlyTiles.width) + 0.5) * cellSize;
	var z = (Math.floor(bestIdx / friendlyTiles.width) + 0.5) * cellSize;

	// default angle
	var angle = 3*Math.PI/4;

	return {
		"x" : x,
		"z" : z,
		"angle" : angle
	};
};
