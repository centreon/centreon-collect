( function( root, factory ) {
	// Set up Backbone-relational for the environment. Start with AMD.
	if ( typeof define === 'function' && define.amd ) {
		define( [ 'exports', 'backbone', 'underscore' ], factory );
	}
	// Next for Node.js or CommonJS.
	else if ( typeof exports !== 'undefined' ) {
		factory( exports, require( 'backbone' ), require( 'underscore' ) );
	}
	// Finally, as a browser global. Use `root` here as it references `window`.
	else {
		factory( root, root.Backbone, root._ );
	}
}( this, function( exports, Backbone, _ ) {
	'use strict';

	var ModelFactory = function() {
		this.registeredModels = {};
	};

	ModelFactory.prototype.registerModel = function(model) {
		this.registeredModels[model.prototype.defaults.type] = model;
	}

	ModelFactory.prototype.findOrCreate = function(data) {
		if (this.registeredModels[data.type])
			return this.registeredModels[data.type].findOrCreate(data, {parse: true});
	}

	ModelFactory.prototype.createFromArray = function(items) {
		_.each(items, function(item) {
			this.findOrCreate(item);
		}, this);
	};

	Backbone.modelFactory = new ModelFactory();

	Backbone.Collection.prototype.parse = function(response) {
		if (!response)
			return;

		if (response.included)
			Backbone.modelFactory.createFromArray(response.included);

		if (response.meta && this.handleMeta)
			this.handleMeta(response.meta);

		if (!response.data) {
			return response;
		}

		return response.data;
	};

	Backbone.RelationalModel.prototype.parse = function(response) {
		if (!response)
			return;

		if (response.included)
			Backbone.modelFactory.createFromArray(response.included);

		if (response.data) {
			response = response.data;
		}

		if (response.meta && this.handleMeta)
			this.handleMeta(response.meta);

		var data = response.attributes || {};
		data.id = response.id;

		if (response.relationships) {
			var simplifiedRelations = _.mapObject(response.relationships, function(value) {
				return value.data;
			});

			_.extend(data, simplifiedRelations);
		}

		return data;
	};

	return Backbone;
}));
